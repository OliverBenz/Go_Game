#pragma once

#include "core/IGameListener.hpp"
#include "core/types.hpp"

#include <mutex>
#include <vector>

namespace go {

struct ListenerEntry {
	IGameListener* listener; //!< Pointer to the listener.
	uint64_t eventMask;      //!< What events the listener cares about.
}

class NotificationHandler {
public:
	void addListener(IGameListener* listener, uint64_t eventMask);
	void remListener(IGameListener* listener);

	//! Signal a game event.
	void signal(Notification event);

private:
	std::mutex m_listenerMutex;
	std::vector<ListenerEntry> m_listeners;
};

} // namespace go