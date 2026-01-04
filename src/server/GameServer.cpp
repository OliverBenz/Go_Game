#include "GameServer.hpp"

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
	callbacks.onConnect    = [this](network::SessionId sessionId) { return onClientConnected(std::move(sessionId)); };
	callbacks.onMessage    = [this](network::SessionId sessionId, const network::Message& payload) { return onClientMessage(std::move(sessionId), payload); };
	callbacks.onDisconnect = [this](network::SessionId sessionId) { onClientDisconnected(std::move(sessionId)); };
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

void GameServer::onClientConnected(network::SessionId sessionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientConnected, .sessionId = std::move(sessionId)});
}

void GameServer::onClientMessage(network::SessionId sessionId, const network::Message& payload) {
	m_eventQueue.Push(ServerEvent{
	        .type      = ServerEventType::ClientMessage,
	        .sessionId = sessionId,
	        .payload   = payload,
	});
}

void GameServer::onClientDisconnected(network::SessionId sessionId) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientDisconnected, .sessionId = sessionId});
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
		m_network.reject(event.sessionId);
		logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Rejected client '{}'.", event.sessionId));
		return; // TODO: Does not handle reconnect.
	}

	// If we have a player free
	if (auto colour = freePlayer()) {
		m_sessions.push_back(PlayerSession{*colour, event.sessionId});

		m_network.send(event.sessionId, "SESSION:" + event.sessionId); // TODO: Move to protocol

		logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' connected from '{}'", event.sessionId, event.payload));
	} else {
		// Already two players
		m_network.reject(event.sessionId);
		logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Rejected client ''.", event.sessionId));
	}

	// Start game
	if (m_sessions.size() == network::MAX_PLAYERS) {
		m_gameThread = std::thread([&] { m_game.run(); });
	}
}

void GameServer::processClientMessage(const ServerEvent& event) {
	if (!isClientConnected(event.sessionId)) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Message from unknown session '{}'.", event.sessionId));
		return;
	}
	const auto player = playerFromSession(event.sessionId);

	// Server event message contains a network event. Parse and handle.
	const auto networkEvent = network::fromMessage(event.payload);
	if (!networkEvent) {
		return;
	}

	{
		static constexpr char LOG_MSG[] = "[GameServer] Message from client '{}': '{}'.";
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Debug, std::format(LOG_MSG, event.sessionId, event.payload));
	}

	std::visit([&](const auto& e) { handleNetworkEvent(player, e); }, *networkEvent);
}

// TODO: Handle reconnect.
void GameServer::processClientDisconnect(const ServerEvent& event) {
	if (!isClientConnected(event.sessionId)) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Disconnect from unknown session '{}'.", event.sessionId));
		return;
	}

	// Remove session
	auto it = std::ranges::find_if(m_sessions, [&](const PlayerSession& ps) { return ps.sessionId == event.sessionId; });
	if (it != m_sessions.end()) {
		m_sessions.erase(it);
	}

	// TODO: Pause game while waiting for reconnect.

	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' disconnected.", event.sessionId));
}

void GameServer::processShutdown(const ServerEvent&) {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, "[GameServer] Shutdown event received.");
	m_game.pushEvent(ShutdownEvent{});
	m_isRunning = false;
}

network::SessionId GameServer::sessionFromPlayer(Player player) const {
	for (const auto& session: m_sessions) {
		if (session.player == player) {
			return session.sessionId;
		}
	}
	return {};
}

Player GameServer::playerFromSession(network::SessionId sessionId) const {
	for (const auto& session: m_sessions) {
		if (session.sessionId == sessionId) {
			return session.player;
		}
	}

	assert(false);
	return Player::Black;
}

bool GameServer::isClientConnected(network::SessionId sessionId) const {
	for (const auto& session: m_sessions) {
		if (session.sessionId == sessionId) {
			return true;
		}
	}
	return false;
}

std::optional<Player> GameServer::freePlayer() const {
	assert(m_sessions.size() <= network::MAX_PLAYERS);

	switch (m_sessions.size()) {
	case 0u:
		return Player::Black;
	case 1u:
		return opponent(m_sessions[0].player);
	default:
		return std::nullopt;
	}
	return std::nullopt;
}

void GameServer::handleNetworkEvent(Player player, const network::NwPutStoneEvent& event) {
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

void GameServer::handleNetworkEvent(Player player, const network::NwPassEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{player});
	logger.Log(Logging::LogLevel::Warning, std::format("[GameServer] Player {} passed.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const network::NwResignEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Player {} resigned.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const network::NwChatEvent& event) {
	// Forward message to opponent and echo to sender for simple client feedback.
	const auto opponentId = sessionFromPlayer(opponent(player));
	if (!opponentId.empty()) {
		m_network.send(opponentId, event.message);
	}

	const auto senderId = sessionFromPlayer(player);
	if (!senderId.empty() && senderId != opponentId) {
		m_network.send(senderId, event.message);
	}
}

} // namespace go::server
