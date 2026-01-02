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

// Generate simple hex session keys. This is intentionally lightweight and local to the server layer.
static std::string CreateSessionKey() {
	std::array<uint8_t, 16> bytes{};
	std::random_device rd;
	for (auto& b: bytes) {
		b = static_cast<uint8_t>(rd());
	}

	std::ostringstream oss;
	for (const auto byte: bytes) {
		oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
	}
	return oss.str();
}

GameServer::GameServer(Game& game, std::uint16_t port) : m_game{game}, m_network{port} {
	// Wire up network callbacks but keep them thin: they only enqueue events.
	m_network.setOnConnect(
	        [this](std::size_t clientIndex, const asio::ip::tcp::endpoint& endpoint) { onClientConnected(clientIndex, endpoint); });
	m_network.setOnMessage([this](std::size_t clientIndex, const std::string& payload) { return onClientMessage(clientIndex, payload); });
	m_network.setOnDisconnect([this](std::size_t clientIndex) { onClientDisconnected(clientIndex); });
}

GameServer::~GameServer() {
	stop();
}

void GameServer::start() {
	if (m_isRunning.exchange(true)) {
		return;
	}

	m_network.start();
	m_serverThread = std::thread([this] { serverLoop(); });
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

	if (m_serverThread.joinable()) {
		m_serverThread.join();
	}
}

void GameServer::onClientConnected(std::size_t clientIndex, const asio::ip::tcp::endpoint& endpoint) {
	// Network thread -> enqueue and return ASAP.
	m_eventQueue.Push(
	        ServerEvent{.type = ServerEventType::ClientConnected, .clientIndex = clientIndex, .payload = endpoint.address().to_string()});
}

// TODO: Client session assigned on connection. Not on message
std::optional<std::string> GameServer::onClientMessage(std::size_t clientIndex, const std::string& payload) {
	const auto session = ensureSession(clientIndex);
	m_eventQueue.Push(ServerEvent{
	        .type        = ServerEventType::ClientMessage,
	        .clientIndex = clientIndex,
	        .payload     = payload,
	        .sessionKey  = session,
	});

	// Send lightweight session echo so the client gets a prompt response while the server thread processes.
	return std::format("SESSION:{}", session);
}

void GameServer::onClientDisconnected(std::size_t clientIndex) {
	m_eventQueue.Push(ServerEvent{.type = ServerEventType::ClientDisconnected, .clientIndex = clientIndex});
}

void GameServer::serverLoop() {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, "[GameServer] Event loop started.");
	logger.Flush();

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

	logger.Log(Logging::LogLevel::Info, "[GameServer] Event loop stopped.");
}

void GameServer::processEvent(const ServerEvent& event) {
	auto logger = Logger();

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

void GameServer::processClientMessage(const ServerEvent& event) {
	static constexpr char LOG_MSG[] = "[GameServer] Message from client {} (session {}): '{}'.";
	auto logger                     = Logger();
	logger.Log(Logging::LogLevel::Debug, std::format(LOG_MSG, event.clientIndex + 1, event.sessionKey, event.payload));

	// Server event message contains a network event. Parse and handle.
	const auto networkEvent = network::fromMessage(event.payload);
	if (!networkEvent) {
		return;
	}
	std::visit([&](const auto& e) { handleNetworkEvent(event, e); }, *networkEvent);
}

void GameServer::processClientConnect(const ServerEvent& event) {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client {} connected from '{}'.", event.clientIndex + 1, event.payload));
}
void GameServer::processClientDisconnect(const ServerEvent& event) {
	auto logger = Logger();
	// TODO: Handle reconnect.
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client {} disconnected.", event.clientIndex + 1));
}
void GameServer::processShutdown(const ServerEvent&) {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, "[GameServer] Shutdown event received.");
	m_isRunning = false;
}

std::string GameServer::ensureSession(std::size_t clientIndex) {
	if (clientIndex >= network::MAX_PLAYERS) {
		return {};
	}

	if (!m_sessions[clientIndex].has_value()) {
		m_sessions[clientIndex] = PlayerSession{CreateSessionKey(), clientIndex};
	}

	return m_sessions[clientIndex]->sessionKey;
}

std::optional<std::size_t> GameServer::opponentIndex(std::size_t clientIndex) const {
	// TODO: Fix usage, then assert and remove optional
	if (clientIndex >= network::MAX_PLAYERS) {
		return std::nullopt;
	}
	return std::optional<std::size_t>{1 - clientIndex};
}

Player GameServer::seatToPlayer(std::size_t clientIndex) const {
	return clientIndex == 0 ? Player::Black : Player::White;
}

void GameServer::handleNetworkEvent(const ServerEvent& srvEvent, const network::NwPutStoneEvent& putEvent) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting PutStone: game is not active.");
		return;
	}

	// Push into the core game loop; legality (ko, captures, etc.) is still enforced there.
	const auto move   = Coord{putEvent.x, putEvent.y};
	const auto player = seatToPlayer(srvEvent.clientIndex);
	logger.Log(Logging::LogLevel::Info,
	           std::format("[GameServer] Accepting PutStone from client {} at ({}, {}).", srvEvent.clientIndex + 1, move.x, move.y));

	m_game.pushEvent(PutStoneEvent{player, move});
}

void GameServer::handleNetworkEvent(const ServerEvent& srvEvent, const network::NwPassEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{seatToPlayer(srvEvent.clientIndex)});
}

void GameServer::handleNetworkEvent(const ServerEvent& srvEvent, const network::NwResignEvent&) {
	auto logger = Logger();

	if (!m_game.isActive()) {
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client {} resigned.", srvEvent.clientIndex + 1));
}

void GameServer::handleNetworkEvent(const ServerEvent& srvEvent, const network::NwChatEvent& chatEvent) {
	const auto opponent = opponentIndex(srvEvent.clientIndex);
	if (!opponent.has_value()) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Chat dropped: opponent not connected.");
		return;
	}

	const std::string envelope = std::format("CHAT_FROM:{}:{}", ensureSession(srvEvent.clientIndex), chatEvent.message);
	m_network.sendToClient(*opponent, envelope);
}

} // namespace go::server
