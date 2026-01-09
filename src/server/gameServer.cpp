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

GameServer::GameServer(std::uint16_t port) : m_server{port} {
}
GameServer::~GameServer() {
	stop();
}

void GameServer::start() {
	m_server.registerHandler(this);
	m_server.start();
}

void GameServer::stop() {
	m_server.stop();

	if (m_gameThread.joinable()) {
		m_gameThread.join();
	}
}

void GameServer::onClientConnected(SessionId sessionId, Seat seat) {
	assert(gameNet::isPlayer(seat));
	const auto player = seat == Seat::Black ? Player::Black : Player::White;
	if (m_game.isActive()) {
		return; // TODO: Reconnect?
	}
	if (m_players.contains(player)) {
		return; // TODO: Handle reconnect.
	}
	m_players.emplace(player, sessionId);

	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' connected.", sessionId));

	if (m_players.size() == 2) // TODO: Better check
	{
		m_gameThread = std::thread([&] { m_game.run(); });
	}
}

void GameServer::onClientDisconnected(SessionId sessionId) {
	// Not handled for now. No timing in game.
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' disconnected.", sessionId));
}

void GameServer::onNetworkEvent(SessionId sessionId, const NwEvent& event) {
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Received event from client '{}'.", sessionId));
	std::visit(
	        [&](const auto& e) {
		        const auto seat = m_server.getSeat(sessionId);
		        if (!gameNet::isPlayer(seat)) {
			        Logger().Log(Logging::LogLevel::Warning,
			                     std::format("[GameServer] Ignoring event from non-player seat for session '{}'.", sessionId));
			        return;
		        }

		        const auto player = seat == Seat::Black ? Player::Black : Player::White;
		        handleNetworkEvent(player, e);
	        },
	        event);
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwPutStoneEvent& event) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting PutStone: game is not active.");
		return;
	}

	// Push into the core game loop; legality (ko, captures, etc.) is still enforced there.
	const auto move = Coord{event.x, event.y};
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Accepting PutStone from player {} at ({}, {}).", static_cast<int>(player), move.x, move.y));

	m_game.pushEvent(PutStoneEvent{player, move});
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwPassEvent&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{player});
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Player {} passed.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::NwResignEvent&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Player {} resigned.", static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player, const gameNet::NwChatEvent& event) {
	m_server.broadcast(event);
}

} // namespace go::server
