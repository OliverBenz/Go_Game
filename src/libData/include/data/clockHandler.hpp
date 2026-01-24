#pragma once

#include "data/clock.hpp"

namespace go {

//! Interface which all clocks require
class ClockHandler {
public:
	using Duration  = Clock::Duration;
	using TimePoint = Clock::TimePoint;

	virtual ~ClockHandler() = default;

	void start(Player toMove, TimePoint now) {
		if (m_state == Clock::State::Running) {
			updateElapsed(now);
		}
		m_running  = toMove;
		m_lastTick = now;
		m_state    = Clock::State::Running;
	}

	void pause(TimePoint now) {
		if (m_state == Clock::State::Running) {
			updateElapsed(now);
			m_state = Clock::State::Paused;
		}
	}

	void stop(TimePoint now) {
		if (m_state == Clock::State::Running) {
			updateElapsed(now);
		}
		m_state = Clock::State::Stopped;
	}

	void resume(TimePoint now) {
		if (m_state == Clock::State::Paused) {
			m_lastTick = now;
			m_state    = Clock::State::Running;
		}
	}

    void reset() {
        
    }

	virtual void push(Player whoMoved, TimePoint now)     = 0;
	virtual Clock::Snapshot snapshot(TimePoint now) const = 0;

protected:
	Player m_running{Player::Black};
	Clock::State m_state{Clock::State::Stopped};

	TimePoint m_lastTick{};
};

} // namespace go
