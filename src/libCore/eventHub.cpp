#include "core/eventHub.hpp"

#include <algorithm>

namespace go {

void EventHub::subscribe(IGameListener* listener, uint64_t signalMask) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_listeners.push_back({listener, signalMask});
}

void EventHub::unsubscribe(IGameListener* listener) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(), [&](const ListenerEntry& e) { return e.listener == listener; }),
	                  m_listeners.end());
}

void EventHub::signal(GameSignal signal) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	for (const auto& [listener, signalMask]: m_listeners) {
		if (signalMask & signal) {
			// TODO: How to ensure these listener side functions are not heavy.
			//       Else they might block the game thread.
			listener->onGameEvent(signal);
		}
	}
}

} // namespace go