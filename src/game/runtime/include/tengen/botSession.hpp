#pragma once

#include "core/IGameStateListener.hpp"
#include "core/game.hpp"
#include "core/gameEvent.hpp"
#include "tengen/IGameSession.hpp"
#include "tengen/eventHub.hpp"
#include "tengen/position.hpp"

#include <mutex>
#include <thread>

namespace tengen::app {

class BotSession : public IGameSession, public IGameStateListener {
	BotSession(const unsigned boardSize, const int difficulty, const bool playerPlaysAsBlack)
	    : m_position(boardSize), m_game(boardSize) {

		// TODO: playerPlaysAsBlack and engine init
		m_position.init(boardSize);
		m_game.subscribeState(this);
		m_gameThread = std::thread([this] { m_game.run(); });
	}
	~BotSession() {
		shutdown();
	}

public: // IGameSession Interface
	GameStatus status() const override {
		std::lock_guard<std::mutex> lock(m_stateMutex);
		return m_position.getStatus();
	}
	Board board() const override {
		std::lock_guard<std::mutex> lock(m_stateMutex);
		return m_position.getBoard();
	}
	Player currentPlayer() const override {
		std::lock_guard<std::mutex> lock(m_stateMutex);
		return m_position.getPlayer();
	}

	void tryPlace(const unsigned x, const unsigned y) override {
		m_game.pushEvent(PutStoneEvent{currentPlayer(), Coord{x, y}});
	}
	void tryPass() override {
		m_game.pushEvent(PassEvent{currentPlayer()});
	}
	void tryResign() override {
		m_game.pushEvent(ResignEvent{});
	}
	void shutdown() override {
		m_game.pushEvent(ShutdownEvent{});
		if (m_gameThread.joinable()) {
			m_gameThread.join();
		}
		m_game.unsubscribeState(this);
	}

public: // IAppSignalSource Interface
	void subscribe(app::IAppSignalListener* listener, uint64_t mask) override {
		m_eventHub.subscribe(listener, mask);
	}
	void unsubscribe(app::IAppSignalListener* listener) override {
		m_eventHub.unsubscribe(listener);
	}

public: // IGameStateListener Interface
	void onGameDelta(const GameDelta& delta) override {
		GameStatus status         = GameStatus::Active;
		GameStatus previousStatus = GameStatus::Active;
		bool applied              = false;
		{
			std::lock_guard<std::mutex> lock(m_stateMutex);
			previousStatus = m_position.getStatus();
			applied        = m_position.apply(delta);
			status         = m_position.getStatus();
		}

		if (!applied) {
			return;
		}

		// TODO: Query bot for his move if his turn.
		switch (delta.action) {
		}
	}

private:
	Game m_game;           //!< Game instance. Run locally on bot games.
	Position m_position{}; //!< Tracks the board state as signalled by the Game.
	EventHub m_eventHub;   //!< Event notifier.

	std::thread m_gameThread;        //!< Runs the game loop.
	mutable std::mutex m_stateMutex; //!< Concurrency handling.
};

} // namespace tengen::app
