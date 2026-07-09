#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace tengen {

class Timer {
public:
	Timer() = default;
	Timer(unsigned ms, std::function<void()> callback);
	~Timer();

	void start();
	void stop();
	bool isActive();
	void setPeriod(const unsigned ms);

private:
	void run(std::stop_token stopToken);

private:
	bool m_isRunning{false};
	unsigned m_period{500u};
	std::function<void()> m_callback{nullptr};

	std::jthread m_timerThread{};
	std::condition_variable_any m_stopCondition{};
};

} // namespace tengen
