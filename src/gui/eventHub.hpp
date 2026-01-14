#pragma once

#include "IGameSignalListener.hpp"

#include <mutex>
#include <vector>

namespace go::gui {

//! Allows external components to be updated on internal game events.
class EventHub {
	struct SignalListenerEntry {
		IGameSignalListener* listener; //!< Pointer to the listener.
		uint64_t signalMask;           //!< What events the listener cares about.
	};

public:
	void subscribe(IGameSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IGameSignalListener* listener);
	void signal(GameSignal signal); //!< Signal a game event.

private:
	std::mutex m_listenerMutex;
	std::vector<SignalListenerEntry> m_signalListeners;
};

} // namespace go::gui
