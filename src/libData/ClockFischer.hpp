#pragma once

#include "ClockAbsolute.hpp"

#include <algorithm>

namespace go {

//! Fischer clock is an absolute clock with increments the time after each push.
class FischerClock final : public AbsoluteClock {
public:
	FischerClock(Duration main, Duration increment) : AbsoluteClock(main), m_increment(std::max(increment, Duration::zero())) {
	}

	void push(Player whoMoved, TimePoint now) override {
		if (!isRunning()) {
			return;
		}
		AbsoluteClock::push(whoMoved, now);
		addMainTime(whoMoved, m_increment);
	}

private:
	Duration m_increment;
};

} // namespace go
