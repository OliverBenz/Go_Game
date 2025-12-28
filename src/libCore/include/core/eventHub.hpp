#pragma once

#include "core/IGameListener.hpp"
#include "core/types.hpp"

#include <mutex>
#include <vector>

namespace go {

//! Allows external components to be updated on internal game events.
class EventHub {
	struct ListenerEntry {
		IGameListener* listener; //!< Pointer to the listener.
		uint64_t signalMask;     //!< What events the listener cares about.
	};

public:
	void subscribe(IGameListener* listener, uint64_t signalMask);
	void unsubscribe(IGameListener* listener);

	//! Signal a game event.
	void signal(GameSignal signal);

private:
	std::mutex m_listenerMutex;
	std::vector<ListenerEntry> m_listeners;
};

} // namespace go