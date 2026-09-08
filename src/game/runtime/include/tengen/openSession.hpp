#pragma once

#include "core/IGameStateListener.hpp"
#include "core/game.hpp"
#include "tengen/IGameSession.hpp"
#include "tengen/eventHub.hpp"
#include "tengen/position.hpp"

#include <mutex>
#include <thread>

namespace tengen::app {

//! Free play locally you control both players.
class OpenSession : public IGameSession, public IGameStateListener {
public:
	OpenSession(std::size_t boardSize);
	~OpenSession() override;

public: // IGameSession Interface
	GameStatus status() const override;
	Board board() const override;
	Player currentPlayer() const override;

	void tryPlace(unsigned x, unsigned y) override;
	void tryPass() override;
	void tryResign() override;
	void shutdown() override;

public: // IAppSignalSource Interface
	void subscribe(app::IAppSignalListener* listener, uint64_t mask) override;
	void unsubscribe(app::IAppSignalListener* listener) override;

public: // IGameStateListener Interface
	void onGameDelta(const GameDelta& delta) override;

private:
	Game m_game;           //!< Game instance. Run locally on open sessions.
	Position m_position{}; //!< Tracks the board state as signalled by the Game.
	EventHub m_eventHub;   //!< Event notifier.

	std::thread m_gameThread;        //!< Runs the game loop.
	mutable std::mutex m_stateMutex; //!< Concurrency handling.
};

} // namespace tengen::app
