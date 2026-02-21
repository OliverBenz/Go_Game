#include "tengen/eventHub.hpp"

#include <algorithm>

namespace tengen::app {

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
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	for (const auto& [listener, signalMask]: m_signalListeners) {
		if (signalMask & signal) {
			listener->onAppEvent(signal);
		}
	}
}

} // namespace tengen::app
