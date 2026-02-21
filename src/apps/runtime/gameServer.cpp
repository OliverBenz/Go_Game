#include "app/gameServer.hpp"

#include "core/game.hpp"
#include "logging.hpp"

#include <cassert>
#include <format>

namespace go::app {

static constexpr char LOG_REC_PUT[]    = "[GameServer] Received Event 'Put'    from player {} at ({}, {}).";
static constexpr char LOG_REC_PASS[]   = "[GameServer] Received Event 'Pass'   from Player {}.";
static constexpr char LOG_REC_RESIGN[] = "[GameServer] Received Event 'Resign' from Player {}.";

GameServer::GameServer(std::size_t boardSize) : m_game(boardSize) {
}
GameServer::~GameServer() {
	stop();
}

void GameServer::start() {
	if (!m_server.registerHandler(this)) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Server handler already registered. Start ignored.");
		return;
	}
	m_game.subscribeState(this);
	m_server.start();
}

void GameServer::stop() {
	if (m_gameThread.joinable()) {
		m_game.pushEvent(ShutdownEvent{});
	}

	m_server.stop();
	m_game.unsubscribeState(this);

	if (m_gameThread.joinable()) {
		m_gameThread.join();
	}
	m_players.clear();
}

void GameServer::onClientConnected(network::SessionId sessionId, network::Seat seat) {
	if (!network::isPlayer(seat)) {
		return;
	}

	const auto player = seat == network::Seat::Black ? Player::Black : Player::White;
	if (m_game.isActive()) {
		return; // TODO: Reconnect?
	}
	if (m_players.contains(player)) {
		return; // TODO: Handle reconnect.
	}
	m_players.emplace(player, sessionId);

	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' connected.", sessionId));

	if (m_players.size() == 2 && !m_gameThread.joinable()) {
		m_gameThread = std::thread([this] { m_game.run(); });

		// TODO: Komi and timer not yet implemented.
		m_server.broadcast(network::ServerGameConfig{
		        .boardSize   = static_cast<unsigned>(m_game.boardSize()),
		        .komi        = 6.5,
		        .timeSeconds = 0u,
		});
	}
}

void GameServer::onClientDisconnected(network::SessionId sessionId) {
	// Not handled for now. No timing in game.
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' disconnected.", sessionId));
	for (auto it = m_players.begin(); it != m_players.end(); ++it) {
		if (it->second == sessionId) {
			m_players.erase(it);
			break;
		}
	}
}

void GameServer::onNetworkEvent(network::SessionId sessionId, const network::ClientEvent& event) {
	std::visit(
	        [&](const auto& e) {
		        const auto seat = m_server.getSeat(sessionId);
		        if (!network::isPlayer(seat)) {
			        Logger().Log(Logging::LogLevel::Warning, std::format("[GameServer] Ignoring event from non-player seat for session '{}'.", sessionId));
			        return;
		        }

		        const auto player = seat == network::Seat::Black ? Player::Black : Player::White;
		        handleNetworkEvent(player, e);
	        },
	        event);
}

void GameServer::onGameDelta(const GameDelta& delta) {
	network::ServerAction action = network::ServerAction::Pass;
	switch (delta.action) {
	case GameAction::Place:
		action = network::ServerAction::Place;
		break;
	case GameAction::Pass:
		action = network::ServerAction::Pass;
		break;
	case GameAction::Resign:
		action = network::ServerAction::Resign;
		break;
	}

	// TODO: Game status: Core cannot count territory yet so game not active is signaled as draw.
	network::ServerDelta updateEvent{
	        .turn     = delta.moveId,
	        .seat     = delta.player == Player::Black ? network::Seat::Black : network::Seat::White,
	        .action   = action,
	        .coord    = delta.coord,
	        .captures = delta.captures,
	        .next     = delta.nextPlayer == Player::Black ? network::Seat::Black : network::Seat::White,
	        .status   = delta.gameActive ? network::GameStatus::Active : network::GameStatus::Draw,
	};

	m_server.broadcast(updateEvent);
}

void GameServer::handleNetworkEvent(Player player, const network::ClientPutStone& event) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting PutStone: game is not active.");
		return;
	}

	// Push into the core game loop; legality (ko, captures, etc.) is still enforced there.
	const auto move = Coord{event.c.x, event.c.y};
	m_game.pushEvent(PutStoneEvent{player, move});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_PUT, static_cast<int>(player), move.x, move.y));
}

void GameServer::handleNetworkEvent(Player player, const network::ClientPass&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{player});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_PASS, static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const network::ClientResign&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_RESIGN, static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const network::ClientChat& event) {
	m_chatHistory.emplace_back(ChatEntry{player, event.message});
	m_server.broadcast(network::ServerChat{player, static_cast<unsigned>(m_chatHistory.size()), event.message});
}

} // namespace go::app
