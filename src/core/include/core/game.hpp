#pragma once

#include "core/IZobristHash.hpp"
#include "core/SafeQueue.hpp"
#include "core/board.hpp"
#include "core/gameEvent.hpp"
#include "core/moveChecker.hpp"
#include "core/notificationHandler.hpp"
#include "core/types.hpp"

namespace go {

using EventQueue = SafeQueue<GameEvent>;

struct Position {
	Position(std::size_t boardSize) : board{boardSize} {
	}
	Board board;
	Player currentPlayer{Player::Black};
	uint64_t hash = 0; //!< Game state hash

	//! \note Assume legal move.
	void putStone(Coord c, IZobristHash& hasher) {
		board.setAt(c, toBoardValue(currentPlayer));
		hash ^= hasher.stone(c, currentPlayer);

		currentPlayer = opponent(currentPlayer);
		hash ^= hasher.togglePlayer();
	}

	void pass(IZobristHash& hasher) {
		currentPlayer = opponent(currentPlayer);
		hash ^= hasher.togglePlayer();
	}
};

//! Core game setup.
class Game {
public:
	//! Setup a game of certain board size without starting the game loop.
	Game(std::size_t boardSize);

	//! Run the main game loop/start handling the event loop.
	void run();

	//! Push an event to the event queue.
	void pushEvent(GameEvent event);

	//! Get board data for rendering.
	const Board& board() const;

public:
	void addNotificationListener(IGameListener* listener);
	void removeNotificationListener(IGameListener* listener);

private:
	void handleEvent(const PutStoneEvent& event);
	void handleEvent(const PassEvent& event);
	void handleEvent(const ResignEvent& event);
	void handleEvent(const ShutdownEvent& event);

private:
	bool m_gameActive;

	Position m_position;
	EventQueue m_eventQueue;

	std::unordered_set<uint64_t> m_seenHashes; //!< History of board states.
	std::unique_ptr<IZobristHash> m_hasher;    //!< Store the last 2 moves. Allows to check repeating board state.

	NotificationHandler m_notificationHandler;
};

} // namespace go