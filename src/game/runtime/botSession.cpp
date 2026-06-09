#include "tengen/botSession.hpp"

#include "core/gameEvent.hpp"

#include <cassert>
#include <utility>

namespace tengen::app {

BotSession::BotSession(BotSessionConfig config, std::unique_ptr<engine::IEngineSession> engineSession)
    : m_config(std::move(config)), m_engine(std::move(engineSession)), m_game(m_config.boardSize) {
	assert(m_engine);

	m_position.init(m_config.boardSize);
	m_game.subscribeState(this);

	if (m_engine) {
		m_engine->newGame(toEngineConfig());
	}

	m_gameThread = std::thread([this] { m_game.run(); });

	if (botPlayer() == Player::Black) {
		maybeRequestBotMove();
	}
}

BotSession::~BotSession() {
	shutdown();
}

GameStatus BotSession::status() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getStatus();
}

Board BotSession::board() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getBoard();
}

Player BotSession::currentPlayer() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getPlayer();
}

void BotSession::tryPlace(const unsigned x, const unsigned y) {
	if (!isHumanTurn()) {
		return;
	}
	m_game.pushEvent(PutStoneEvent{m_config.humanPlayer, Coord{x, y}});
}

void BotSession::tryPass() {
	if (!isHumanTurn()) {
		return;
	}
	m_game.pushEvent(PassEvent{m_config.humanPlayer});
}

void BotSession::tryResign() {
	if (status() != GameStatus::Active) {
		return;
	}
	m_game.pushEvent(ResignEvent{});
}

void BotSession::shutdown() {
	m_game.pushEvent(ShutdownEvent{});
	if (m_gameThread.joinable()) {
		m_gameThread.join();
	}
	m_game.unsubscribeState(this);
	if (m_engine) {
		m_engine->shutdown();
	}
}

void BotSession::subscribe(IAppSignalListener* listener, const uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void BotSession::unsubscribe(IAppSignalListener* listener) {
	m_eventHub.unsubscribe(listener);
}

void BotSession::onGameDelta(const GameDelta& delta) {
	GameStatus status         = GameStatus::Active;
	GameStatus previousStatus = GameStatus::Active;
	bool applied              = false;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		previousStatus = m_position.getStatus();
		applied        = m_position.apply(delta);
		status         = m_position.getStatus();
		if (applied) {
			m_history.push_back(delta);
		}
	}

	if (!applied) {
		return;
	}

	if (m_engine) {
		const auto move = toEngineMove(delta);
		if (move) {
			m_engine->recordMove(*move);
		}
	}

	switch (delta.action) {
	case GameAction::Place:
		m_eventHub.signal(AS_BoardChange);
		m_eventHub.signal(AS_PlayerChange);
		break;
	case GameAction::Pass:
		m_eventHub.signal(AS_PlayerChange);
		break;
	case GameAction::Resign:
		break;
	}
	if (previousStatus != status) {
		m_eventHub.signal(AS_StateChange);
	}

	if (delta.gameActive && delta.nextPlayer == botPlayer()) {
		maybeRequestBotMove();
	}
}

bool BotSession::isHumanTurn() const {
	return status() == GameStatus::Active && currentPlayer() == m_config.humanPlayer;
}

Player BotSession::botPlayer() const {
	return opponent(m_config.humanPlayer);
}

engine::GameConfig BotSession::toEngineConfig() const {
	return engine::GameConfig{
	        .boardSize  = m_config.boardSize,
	        .komi       = m_config.komi,
	        .rules      = m_config.rules,
	        .difficulty = m_config.difficulty,
	        .limits     = m_config.limits,
	};
}

std::optional<engine::Move> BotSession::toEngineMove(const GameDelta& delta) {
	switch (delta.action) {
	case GameAction::Place:
		if (!delta.coord) {
			return std::nullopt;
		}
		return engine::Move{
		        .kind   = engine::MoveKind::Place,
		        .player = delta.player,
		        .coord  = delta.coord,
		};
	case GameAction::Pass:
		return engine::Move{
		        .kind   = engine::MoveKind::Pass,
		        .player = delta.player,
		        .coord  = std::nullopt,
		};
	case GameAction::Resign:
		return engine::Move{
		        .kind   = engine::MoveKind::Resign,
		        .player = delta.player,
		        .coord  = std::nullopt,
		};
	default:
		assert(false);
		return std::nullopt;
	}
}

void BotSession::maybeRequestBotMove() {
	if (!m_engine || status() != GameStatus::Active || currentPlayer() != botPlayer()) {
		return;
	}

	// TODO: Move this blocking call onto a dedicated worker once the real subprocess transport exists.
	const auto decision = m_engine->requestMove(botPlayer());
	if (!decision) {
		return;
	}

	applyEngineDecision(*decision);
}

void BotSession::applyEngineDecision(const engine::Decision& decision) {
	if (decision.move.player != botPlayer()) {
		return;
	}

	switch (decision.move.kind) {
	case engine::MoveKind::Place:
		if (!decision.move.coord) {
			return;
		}
		m_game.pushEvent(PutStoneEvent{decision.move.player, *decision.move.coord});
		break;
	case engine::MoveKind::Pass:
		m_game.pushEvent(PassEvent{decision.move.player});
		break;
	case engine::MoveKind::Resign:
		m_game.pushEvent(ResignEvent{});
		break;
	default:
		assert(false);
		break;
	}
}

} // namespace tengen::app
