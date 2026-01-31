#include "app/gameServer.hpp"

#include "Logging.hpp"
#include "core/game.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <format>
#include <iomanip>
#include <random>
#include <sstream>

namespace go::app {

static constexpr char LOG_REC_PUT[]    = "[GameServer] Received Event 'Put'    from player {} at ({}, {}).";
static constexpr char LOG_REC_PASS[]   = "[GameServer] Received Event 'Pass'   from Player {}.";
static constexpr char LOG_REC_RESIGN[] = "[GameServer] Received Event 'Resign' from Player {}.";
static constexpr std::size_t kMaxChatMessageBytes = 256u;

namespace {

std::string sanitizeChatMessage(std::string message) {
	message.erase(std::remove_if(message.begin(), message.end(), [](unsigned char c) { return c == '\r' || c == '\n'; }), message.end());
	auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
	auto first   = std::find_if_not(message.begin(), message.end(), isSpace);
	if (first == message.end()) {
		return {};
	}
	auto last = std::find_if_not(message.rbegin(), message.rend(), isSpace).base();
	message.assign(first, last);
	if (message.size() > kMaxChatMessageBytes) {
		message.resize(kMaxChatMessageBytes);
	}
	return message;
}

} // namespace

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
	m_server.stop();
	m_game.unsubscribeState(this);

	if (m_gameThread.joinable()) {
		m_game.pushEvent(ShutdownEvent{});
		m_gameThread.join();
	}
}

void GameServer::onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) {
	if (!gameNet::isPlayer(seat)) {
		return;
	}

	const auto player = seat == gameNet::Seat::Black ? Player::Black : Player::White;
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
		m_server.broadcast(gameNet::ServerGameConfig{
		        .boardSize   = static_cast<unsigned>(m_game.boardSize()),
		        .komi        = 6.5,
		        .timeSeconds = 0u,
		});
	}
}

void GameServer::onClientDisconnected(gameNet::SessionId sessionId) {
	// Not handled for now. No timing in game.
	Logger().Log(Logging::LogLevel::Info, std::format("[GameServer] Client '{}' disconnected.", sessionId));
	for (auto it = m_players.begin(); it != m_players.end(); ++it) {
		if (it->second == sessionId) {
			m_players.erase(it);
			break;
		}
	}
}

void GameServer::onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) {
	std::visit(
	        [&](const auto& e) {
		        const auto seat = m_server.getSeat(sessionId);
		        if (!gameNet::isPlayer(seat)) {
			        Logger().Log(Logging::LogLevel::Warning, std::format("[GameServer] Ignoring event from non-player seat for session '{}'.", sessionId));
			        return;
		        }

		        const auto player = seat == gameNet::Seat::Black ? Player::Black : Player::White;
		        handleNetworkEvent(player, e);
	        },
	        event);
}

void GameServer::onGameDelta(const GameDelta& delta) {
	gameNet::ServerAction action = gameNet::ServerAction::Pass;
	switch (delta.action) {
	case GameAction::Place:
		action = gameNet::ServerAction::Place;
		break;
	case GameAction::Pass:
		action = gameNet::ServerAction::Pass;
		break;
	case GameAction::Resign:
		action = gameNet::ServerAction::Resign;
		break;
	}

	// TODO: Game status: Core cannot count territory yet so game not active is signaled as draw.
	gameNet::ServerDelta updateEvent{
	        .turn     = delta.moveId,
	        .seat     = delta.player == Player::Black ? gameNet::Seat::Black : gameNet::Seat::White,
	        .action   = action,
	        .coord    = delta.coord,
	        .captures = delta.captures,
	        .next     = delta.nextPlayer == Player::Black ? gameNet::Seat::Black : gameNet::Seat::White,
	        .status   = delta.gameActive ? gameNet::GameStatus::Active : gameNet::GameStatus::Draw,
	};

	m_server.broadcast(updateEvent);
}

void GameServer::handleNetworkEvent(Player player, const gameNet::ClientPutStone& event) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting PutStone: game is not active.");
		return;
	}

	// Push into the core game loop; legality (ko, captures, etc.) is still enforced there.
	const auto move = Coord{event.c.x, event.c.y};
	m_game.pushEvent(PutStoneEvent{player, move});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_PUT, static_cast<int>(player), move.x, move.y));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::ClientPass&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Pass: game is not active.");
		return;
	}

	m_game.pushEvent(PassEvent{player});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_PASS, static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::ClientResign&) {
	if (!m_game.isActive()) {
		Logger().Log(Logging::LogLevel::Warning, "[GameServer] Rejecting Resign: game already inactive.");
		return;
	}

	m_game.pushEvent(ResignEvent{});
	Logger().Log(Logging::LogLevel::Info, std::format(LOG_REC_RESIGN, static_cast<int>(player)));
}

void GameServer::handleNetworkEvent(Player player, const gameNet::ClientChat& event) {
	auto message = sanitizeChatMessage(event.message);
	if (message.empty()) {
		return;
	}
	m_server.broadcast(gameNet::ServerChat{
	        .seat    = (player == Player::Black ? gameNet::Seat::Black : gameNet::Seat::White),
	        .message = std::move(message),
	});
}

} // namespace go::app
