#pragma once

#include "core/IZobristHash.hpp"
#include "core/SafeQueue.hpp"
#include "core/eventHub.hpp"
#include "core/gameEvent.hpp"
#include "core/position.hpp"

#include <unordered_set>

namespace go {

using EventQueue = SafeQueue<GameEvent>;

//! Core game setup.
//! This owns the rules loop and emits deltas; external code should only push events and listen.
class Game {
public:
	//! Setup a game of certain board size without starting the game loop.
	Game(std::size_t boardSize);

	void run();                      //!< Run the main game loop/start handling the event loop (blocking).
	void pushEvent(GameEvent event); //!< Push an event to the event queue.
	bool isActive() const;           //!< Return if the game is active or not.

	std::size_t boardSize() const;

public:
	void subscribeSignals(IGameSignalListener* listener, uint64_t signalMask);
	void unsubscribeSignals(IGameSignalListener* listener);
	void subscribeState(IGameStateListener* listener);
	void unsubscribeState(IGameStateListener* listener);

private:
	void handleEvent(const PutStoneEvent& event);
	void handleEvent(const PassEvent& event);
	void handleEvent(const ResignEvent& event);
	void handleEvent(const ShutdownEvent& event);

private:
	bool m_gameActive;
	unsigned m_consecutivePasses{0}; //!< Two consequtive passes ends game.

	GamePosition m_position;
	EventQueue m_eventQueue; //!< Queue of internal game events we have to handle.
	EventHub m_eventHub;     //!< Hub to signal updates of the game state to external components.

	std::unordered_set<uint64_t> m_seenHashes; //!< History of board states.
	std::unique_ptr<IZobristHash> m_hasher;    //!< Store the last 2 moves. Allows to check repeating board state.
};

} // namespace go
