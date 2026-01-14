#include "eventHub.hpp"

#include <algorithm>

namespace go::gui {

void EventHub::subscribe(IGameSignalListener* listener, uint64_t signalMask) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_signalListeners.push_back({listener, signalMask});
}

void EventHub::unsubscribe(IGameSignalListener* listener) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_signalListeners.erase(
	        std::remove_if(m_signalListeners.begin(), m_signalListeners.end(), [&](const SignalListenerEntry& e) { return e.listener == listener; }),
	        m_signalListeners.end());
}

void EventHub::signal(GameSignal signal) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	for (const auto& [listener, signalMask]: m_signalListeners) {
		if (signalMask & signal) {
			listener->onGameEvent(signal);
		}
	}
}

} // namespace go::gui
