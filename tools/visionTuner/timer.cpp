#include "timer.hpp"

namespace tengen {

Timer::Timer(unsigned ms, std::function<void()> callback) : m_period(std::move(ms)), m_callback(std::move(callback)) {
}

Timer::~Timer() {
	m_callback = nullptr;
	stop();
}

void Timer::start() {
	assert(m_callback); // Why start when we do nothing? Add a callback.
	if (!m_isRunning) {
		m_timerThread = std::jthread([this](std::stop_token stopToken) { run(stopToken); });
		m_isRunning   = true;
	}
}

void Timer::stop() {
	if (m_isRunning) {
		m_timerThread.request_stop();
		m_isRunning = false;
	}
}

bool Timer::isActive() {
	return m_isRunning;
}

void Timer::setPeriod(const unsigned ms) {
	m_period = ms;
}

void Timer::run(std::stop_token stopToken) {
	std::mutex m;
	std::unique_lock lock(m);

	while (!stopToken.stop_requested()) {
		if (m_callback) {
			std::thread(m_callback).detach();
		}

		m_stopCondition.wait_for(lock, stopToken, std::chrono::milliseconds(m_period), [] { return false; });

		if (stopToken.stop_requested()) {
			break;
		}
	}
}

} // namespace tengen
