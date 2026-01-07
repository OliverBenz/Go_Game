#include "gameServer.hpp"

#include "Logging.hpp"
#include "core/game.hpp"
#include "network/protocol.hpp"

#include <asio/ip/tcp.hpp>

#include <format>
#include <iomanip>
#include <random>
#include <sstream>

namespace go::server {

GameServer::GameServer(std::uint16_t port) : m_network{port} {
	// Wire up network callbacks but keep them thin: they only enqueue events.
	network::TcpServer::Callbacks callbacks;
	callbacks.onConnect    = [this](network::ConnectionId connectionId) { return onClientConnected(connectionId); };
	callbacks.onMessage    = [this](network::ConnectionId connectionId, const network::Message& payload) { return onClientMessage(connectionId, payload); };
	callbacks.onDisconnect = [this](network::ConnectionId connectionId) { onClientDisconnected(connectionId); };
	m_network.connect(callbacks);
}

GameServer::~GameServer() {
	stop();
}

void GameServer::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	m_network.start();
	serverLoop();
}

void GameServer::stop() {
	if (!m_isRunning.exchange(false)) {
		return;
	}

	// Wake serverLoop and stop network.
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::Shutdown});
	m_network.stop();

	try {
		m_eventQueue.Release();
	} catch (...) {
		// Release never throws, but keep intent explicit.
	}

	if (m_gameThread.joinable()) {
		m_gameThread.join();
	}
}

void GameServer::onClientConnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientConnected, .connectionId = connectionId});
}

void GameServer::onClientMessage(network::ConnectionId connectionId, const network::Message& payload) {
	m_eventQueue.Push(ServerEvent{
	        .type         = ServerEventType::ClientMessage,
	        .connectionId = connectionId,
	        .payload      = payload,
	});
}

void GameServer::onClientDisconnected(network::ConnectionId connectionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientDisconnected, .connectionId = connectionId});
}

void GameServer::serverLoop() {
	{
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Info, "[GameServer] Event loop started.");
	}

	while (m_isRunning) {
		try {
			const auto event = m_eventQueue.Pop();
			processEvent(event);
		} catch (const std::exception&) {
			if (!m_isRunning) {
				break;
			}
		}
	}

	{
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Info, "[GameServer] Event loop stopped.");
	}
}

void GameServer::processEvent(const ServerEvent& event) {
	switch (event.type) {
	case ServerEventType::ClientConnected:
		processClientConnect(event);
		break;
	case ServerEventType::ClientDisconnected:
		processClientDisconnect(event);
		break;
	case ServerEventType::ClientMessage:
		processClientMessage(event);
		break;
	case ServerEventType::Shutdown:
		processShutdown(event);
		break;
	}
}

void GameServer::processClientConnect(const ServerEvent& event) {
	auto logger = Logger();

	if (m_game.isActive()) {
		m_network.reject(event.connectionId);
		logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Rejected client '{}'.", event.connectionId));
		return; // TODO: Does not handle reconnect or observers.
	}

	// TODO: Possible to have this connectionId already registered?
	const auto sessionId = m_sessionManager.add(event.connectionId);

	// If we have a player free
	if (auto colour = freePlayer()) {
		m_sessionManager.setSeat(sessionId, *colour == Player::Black ? Seat::Black : Seat::White);
		m_network.send(event.connectionId, "SESSION:" + std::to_string(sessionId)); // TODO: Move to protocol
	} else {
		// Already two players. Observer now.
		m_sessionManager.setSeat(sessionId, Seat::Observer);
	}
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' connected.", sessionId));

	// Start game
	if (m_sessionManager.gameReady()) {
		m_gameThread = std::thread([&] { m_game.run(); });
	}
}

void GameServer::processClientMessage(const ServerEvent& event) {
	const auto sessionId = m_sessionManager.getSessionId(event.connectionId);
	if (!sessionId) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Message from connection '{}' without session.", event.connectionId));
		return;
	}
	const auto seat = m_sessionManager.getSeat(sessionId);
	if (seat != Seat::Black && seat != Seat::White) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Message from observer session '{}'.", sessionId));
		return;
	}

	// Server event message contains a network event. Parse and handle.
	const auto networkEvent = gameNet::fromMessage(event.payload);
	if (!networkEvent) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Could not parse client '{}' payload: '{}'.", sessionId, event.payload));
		return;
	}

	{
		static constexpr char LOG_MSG[] = "[GameServer] Message from client '{}': '{}'.";
		auto logger                     = Logger();
		logger.Log(Logging::LogLevel::Debug, std::format(LOG_MSG, event.connectionId, event.payload));
	}

	std::visit([&](const auto& e) { handleNetworkEvent(seat == Seat::Black ? Player::Black : Player::White, e); }, *networkEvent);
}

void GameServer::processClientDisconnect(const ServerEvent& event) {
	const auto sessionId = m_sessionManager.getSessionId(event.connectionId);
	if (!sessionId) {
		// Should never happen
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Error, "[GameServer] Connection ID not mapped to any session ID on disconnect.");
		return;
	}

	// Remove session
	m_sessionManager.setDisconnected(sessionId);
	if (!m_game.isActive()) {
		m_sessionManager.remove(sessionId); // Allow new player to join before game started.
	}

	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' disconnected.", sessionId));
}

void GameServer::processShutdown(const ServerEvent&) {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, "[GameServer] Shutdown event received.");
	m_game.pushEvent(ShutdownEvent{});
	m_isRunning = false;
}

std::optional<Player> GameServer::freePlayer() const {
	if (!m_sessionManager.getConnectionIdBySeat(Seat::Black)) {
		return Player::Black;
	}
	if (!m_sessionManager.getConnectionIdBySeat(Seat::White)) {
		return Player::White;
	}
	return std::nullopt;
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwPutStoneEvent& event) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting PutStone: game is not active.");
		return;
	}

	// Push into the core game loop; legality (ko, captures, etc.) is still enforced there.
	const auto move = Coord{event.x, event.y};
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Accepting PutStone from player {} at ({}, {}).", static_cast<int>(player), move.x, move.y));

	m_game.pushEvent(PutStoneEvent{player, move});
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwPassEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{player});
	logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Player {} passed.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwResignEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Player {} resigned.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwChatEvent& event) {
	// Forward message to opponent and echo to sender for simple client feedback.
	const auto opponentSeat = opponent(player) == Player::Black ? Seat::Black : Seat::White;
	const auto opponentId   = m_sessionManager.getConnectionIdBySeat(opponentSeat);
	if (opponentId != 0) {
		m_network.send(opponentId, event.message);
	}
}

} // namespace go::server
