#include "app/eventHub.hpp"

#include <algorithm>

namespace go::app {

void EventHub::subscribe(IAppSignalListener* listener, uint64_t signalMask) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_signalListeners.push_back({listener, signalMask});
}

void EventHub::unsubscribe(IAppSignalListener* listener) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_signalListeners.erase(
	        std::remove_if(m_signalListeners.begin(), m_signalListeners.end(), [&](const SignalListenerEntry& e) { return e.listener == listener; }),
	        m_signalListeners.end());
}

void EventHub::signal(AppSignal signal) {
	std::vector<SignalListenerEntry> listeners;
	{
		std::lock_guard<std::mutex> lock(m_listenerMutex);
		listeners = m_signalListeners;
	}

	for (const auto& [listener, signalMask]: listeners) {
		if (signalMask & signal) {
			listener->onAppEvent(signal);
		}
	}
}

} // namespace go::app
