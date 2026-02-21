#pragma once

#include "core/IGameSignalListener.hpp"
#include "core/IGameStateListener.hpp"

#include <mutex>
#include <vector>

namespace tengen {

//! Allows external components to be updated on internal game events.
//! \note Signals are synchronous and run on the caller thread.
class EventHub {
	struct SignalListenerEntry {
		IGameSignalListener* listener; //!< Pointer to the listener.
		uint64_t signalMask;           //!< What events the listener cares about.
	};
	struct StateListenerEntry {
		IGameStateListener* listener; //!< Pointer to the listener.
	};

public:
	void subscribe(IGameSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IGameSignalListener* listener);

	void subscribe(IGameStateListener* listener);
	void unsubscribe(IGameStateListener* listener);

	void signal(GameSignal signal);           //!< Signal a game event.
	void signalDelta(const GameDelta& delta); //!< Signal a game state delta.

private:
	std::mutex m_listenerMutex;
	std::vector<SignalListenerEntry> m_signalListeners;
	std::vector<StateListenerEntry> m_stateListeners;
};

} // namespace tengen
