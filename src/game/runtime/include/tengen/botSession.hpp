#pragma once

#include "core/IGameStateListener.hpp"
#include "core/game.hpp"
#include "engine/IEngineSession.hpp"
#include "tengen/IGameSession.hpp"
#include "tengen/eventHub.hpp"
#include "tengen/position.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tengen::app {

struct BotSessionConfig {
	std::size_t boardSize{9u};
	double komi{6.5};
	std::string rules{"chinese"};
	Player humanPlayer{Player::Black};
	engine::Difficulty difficulty{engine::Difficulty::Medium};
	engine::SearchLimits limits{};
};

//! Local game session that delegates only move selection to an external engine backend.
class BotSession : public IGameSession, public IGameStateListener {
public:
	BotSession(BotSessionConfig config, std::unique_ptr<engine::IEngineSession> engineSession);
	~BotSession() override;

	GameStatus status() const override;
	Board board() const override;
	Player currentPlayer() const override;

	void tryPlace(unsigned x, unsigned y) override;
	void tryPass() override;
	void tryResign() override;
	void shutdown() override;

	void subscribe(app::IAppSignalListener* listener, uint64_t signalMask) override;
	void unsubscribe(app::IAppSignalListener* listener) override;

	void onGameDelta(const GameDelta& delta) override;

private:
	bool isHumanTurn() const;
	Player botPlayer() const;
	engine::GameConfig toEngineConfig() const;
	static std::optional<engine::Move> toEngineMove(const GameDelta& delta);

	void maybeRequestBotMove();
	void applyEngineDecision(const engine::Decision& decision);

private:
	BotSessionConfig m_config{};
	std::unique_ptr<engine::IEngineSession> m_engine{nullptr};
	Game m_game;
	EventHub m_eventHub;

	std::thread m_gameThread;
	mutable std::mutex m_stateMutex;

	Position m_position{};
	std::vector<GameDelta> m_history{};
};

} // namespace tengen::app
