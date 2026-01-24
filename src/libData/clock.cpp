#include "data/clock.hpp"

#include "ClockAbsolute.hpp"
#include "ClockByoYomi.hpp"
#include "ClockFischer.hpp"

#include <cassert>
#include <chrono>
#include <type_traits>

namespace go {

Clock::Clock(const Config& config) {
	std::visit(
	        [this](const auto& cfg) {
		        using T = std::decay_t<decltype(cfg)>;

		        if constexpr (std::is_same_v<T, Absolute>) {
			        m_handler = std::make_unique<AbsoluteClock>(cfg.mainTime);
		        } else if constexpr (std::is_same_v<T, Fischer>) {
			        m_handler = std::make_unique<FischerClock>(cfg.mainTime, cfg.increment);
		        } else if constexpr (std::is_same_v<T, ByoYomi>) {
			        m_handler = std::make_unique<ByoYomiClock>(cfg.mainTime, cfg.period, cfg.periods);
		        } else {
			        assert(false); // Not implemented
		        }
	        },
	        config);
}
Clock::~Clock() = default;

void Clock::start(Player p) {
	assert(m_handler);
	m_handler->start(p, std::chrono::steady_clock::now());
}
void Clock::push(Player p) {
	assert(m_handler);
	m_handler->push(p, std::chrono::steady_clock::now());
}
void Clock::pause() {
	assert(m_handler);
	m_handler->pause(std::chrono::steady_clock::now());
}
void Clock::resume() {
	assert(m_handler);
	m_handler->resume(std::chrono::steady_clock::now());
}
void Clock::stop() {
	assert(m_handler);
	m_handler->stop(std::chrono::steady_clock::now());
}
Clock::Snapshot Clock::snapshot() const {
	assert(m_handler);
	return m_handler->snapshot(std::chrono::steady_clock::now());
}


} // namespace go
