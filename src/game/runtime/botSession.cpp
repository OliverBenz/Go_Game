#include "tengen/botSession.hpp"

#include "core/gameEvent.hpp"
#include "logging.hpp"

#include <cassert>
#include <utility>

namespace tengen::app {

BotSession::BotSession(BotSessionConfig config, std::unique_ptr<engine::IEngineSession> engineSession)
    : m_config(std::move(config)), m_engine(std::move(engineSession)), m_game(m_config.boardSize) {
	assert(m_engine);

	m_position.init(m_config.boardSize);
	m_game.subscribeState(this);

	m_gameThread = std::thread([this] { m_game.run(); });
	m_botThread  = std::thread([this] { botLoop(); });

	if (m_engine && m_engine->newGame(toEngineConfig())) {
		m_engineAvailable = true;
	} else if (m_engine) {
		Logger().Log(Logging::LogLevel::Error, "Could not initialize bot engine session: " + m_engine->lastError());
	}

	if (m_engineAvailable && botPlayer() == Player::Black) {
		queueBotMove();
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
	{
		std::lock_guard<std::mutex> lock(m_botMutex);
		m_botShutdownRequested = true;
		m_botMovePending       = false;
	}
	m_botCv.notify_all();

	m_game.pushEvent(ShutdownEvent{});

	if (m_botThread.joinable()) {
		m_botThread.join();
	}
	if (m_gameThread.joinable()) {
		m_gameThread.join();
	}

	m_game.unsubscribeState(this);

	if (m_engine) {
		m_engine->shutdown();
	}
	m_engineAvailable = false;
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

	if (m_engineAvailable) {
		const auto move = toEngineMove(delta);
		if (move && !m_engine->recordMove(*move)) {
			m_engineAvailable = false;
			Logger().Log(Logging::LogLevel::Error, "Could not mirror move to bot engine: " + m_engine->lastError());
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

	if (m_engineAvailable && delta.gameActive && delta.nextPlayer == botPlayer()) {
		queueBotMove();
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

void BotSession::queueBotMove() {
	{
		std::lock_guard<std::mutex> lock(m_botMutex);
		if (m_botShutdownRequested || m_botMovePending) {
			return;
		}
		m_botMovePending = true;
	}
	m_botCv.notify_one();
}

void BotSession::botLoop() {
	while (true) {
		{
			std::unique_lock<std::mutex> lock(m_botMutex);
			m_botCv.wait(lock, [this] { return m_botShutdownRequested || m_botMovePending; });
			if (m_botShutdownRequested) {
				return;
			}
			m_botMovePending = false;
		}

		if (!m_engineAvailable || !m_engine) {
			continue;
		}

		const auto decision = m_engine->requestMove(botPlayer());
		if (!decision) {
			m_engineAvailable = false;
			Logger().Log(Logging::LogLevel::Error, "Could not request move from bot engine: " + m_engine->lastError());
			continue;
		}

		applyEngineDecision(*decision);
	}
}

void BotSession::applyEngineDecision(const engine::Decision& decision) {
	if (decision.move.player != botPlayer()) {
		return;
	}
	if (status() != GameStatus::Active || currentPlayer() != botPlayer()) {
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
