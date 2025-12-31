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
		logger.Log(Logging::LogLevel::Info,
		           std::format("[GameServer] Client {} connected from '{}'.", event.clientIndex + 1, event.payload));
		break;
	case ServerEventType::ClientDisconnected:
		// TODO: Handle reconnect.
		logger.Log(Logging::LogLevel::Info, std::format("[GameServer] Client {} disconnected.", event.clientIndex + 1));
		break;
	case ServerEventType::ClientMessage:
		processClientMessage(event);
		break;
	case ServerEventType::Shutdown:
		logger.Log(Logging::LogLevel::Debug, "[GameServer] Shutdown event received.");
		m_isRunning = false;
		break;
	}
}

void GameServer::processClientMessage(const ServerEvent& event) {
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Debug, std::format("[GameServer] Message from client {} (session {}): '{}'.", event.clientIndex + 1,
	                                                 event.sessionKey, event.payload));

	// Outline: CHAT:<text> is forwarded to the opponent. Future message types can be parsed here.
	static constexpr std::string_view chatPrefix = "CHAT:";
	if (event.payload.rfind(chatPrefix, 0) == 0) {
		forwardChat(event.clientIndex, event.payload.substr(chatPrefix.size()));
		return;
	}

	// Parse payload into core GameEvent types (AttemptPutStone, Pass, Resign) and push into m_game.
	// Simple text protocol for now: PUT:x,y
	static constexpr std::string_view putPrefix = "PUT:";
	if (event.payload.rfind(putPrefix, 0) == 0) {
		ServerEvent putEvent = event;
		const auto content   = event.payload.substr(putPrefix.size());

		// Expect "x,y"
		auto commaPos = content.find(',');
		if (commaPos == std::string::npos) {
			logger.Log(Logging::LogLevel::Error, "[GameServer] Malformed PUT payload (missing comma).");
			return;
		}

		try {
			const auto x = static_cast<unsigned>(std::stoul(content.substr(0, commaPos)));
			const auto y = static_cast<unsigned>(std::stoul(content.substr(commaPos + 1)));

			handleTryPutStone(putEvent.clientIndex, Coord{x, y});
		} catch (const std::exception&) { logger.Log(Logging::LogLevel::Error, "[GameServer] Malformed PUT payload (invalid numbers)."); }

		return;
	}

	// TODO: Pass / Resign messages can be parsed similarly (PASS / RESIGN).
}

void GameServer::handleTryPutStone(std::size_t clientIndex, const Coord& c) {
	auto logger = Logger();

	// Attach session key and enqueue into game; Game decides legality.
	logger.Log(Logging::LogLevel::Info,
	           std::format("[GameServer] Attempting PutStone from client {} at ({}, {}).", clientIndex + 1, c.x, c.y));

	// TODO: Only push if isPlayerTurn(clientIndex), etc
	m_game.pushEvent(PutStoneEvent{c});
}

void GameServer::forwardChat(std::size_t fromIndex, std::string_view message) {
	const auto opponent = opponentIndex(fromIndex);
	if (!opponent.has_value()) {
		auto logger = Logger();
		logger.Log(Logging::LogLevel::Warning, "[GameServer] Chat dropped: opponent not connected.");
		return;
	}

	const std::string envelope = std::format("CHAT_FROM:{}:{}", ensureSession(fromIndex), message);
	m_network.sendToClient(*opponent, envelope);
}

std::string GameServer::ensureSession(std::size_t clientIndex) {
	if (clientIndex >= m_sessions.size()) {
		return {};
	}

	if (!m_sessions[clientIndex].has_value()) {
		m_sessions[clientIndex] = PlayerSession{CreateSessionKey(), clientIndex};
	}

	return m_sessions[clientIndex]->sessionKey;
}

std::optional<std::size_t> GameServer::opponentIndex(std::size_t clientIndex) const {
	// TODO: Fix usage, then assert and remove optional
	if (clientIndex >= m_sessions.size()) {
		return std::nullopt;
	}
	return clientIndex == 0 ? std::optional<std::size_t>{1} : std::optional<std::size_t>{0};
}

} // namespace go::server
