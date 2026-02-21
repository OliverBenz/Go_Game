#include "core/eventHub.hpp"

#include <algorithm>

namespace tengen {

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

void EventHub::subscribe(IGameStateListener* listener) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_stateListeners.push_back({listener});
}

void EventHub::unsubscribe(IGameStateListener* listener) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	m_stateListeners.erase(
	        std::remove_if(m_stateListeners.begin(), m_stateListeners.end(), [&](const StateListenerEntry& e) { return e.listener == listener; }),
	        m_stateListeners.end());
}

void EventHub::signal(GameSignal signal) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	for (const auto& [listener, signalMask]: m_signalListeners) {
		if (signalMask & signal) {
			// Listener callbacks run on the caller thread; keep them light.
			listener->onGameEvent(signal);
		}
	}
}

void EventHub::signalDelta(const GameDelta& delta) {
	std::lock_guard<std::mutex> lock(m_listenerMutex);

	for (const auto& entry: m_stateListeners) {
		entry.listener->onGameDelta(delta);
	}
}

} // namespace tengen
