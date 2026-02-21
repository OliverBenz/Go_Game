#pragma once

#include "IAppSignalListener.hpp"

#include <mutex>
#include <vector>

namespace tengen::app {

//! Allows external components to be updated on internal game events.
class EventHub {
	struct SignalListenerEntry {
		IAppSignalListener* listener; //!< Pointer to the listener.
		uint64_t signalMask;          //!< What events the listener cares about.
	};

public:
	void subscribe(IAppSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IAppSignalListener* listener);
	void signal(AppSignal signal); //!< Signal a game event.

private:
	std::mutex m_listenerMutex;
	std::vector<SignalListenerEntry> m_signalListeners;
};

} // namespace tengen::app
