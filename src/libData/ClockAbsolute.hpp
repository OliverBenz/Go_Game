#pragma once

#include "ClockHandler.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

namespace go {

class AbsoluteClock : public ClockHandler {
public:
	explicit AbsoluteClock(Duration main) : m_black(std::max(Duration::zero(), main)), m_white(std::max(Duration::zero(), main)) {
	}

	void push(Player whoMoved, TimePoint now) override {
		if (m_state != Clock::State::Running) {
			return;
		}

		updateElapsed(now);
		assert(m_running == whoMoved);
		m_running  = opponent(whoMoved);
		m_lastTick = now;
	}

	Clock::Snapshot snapshot(TimePoint now) const override {
		auto black = m_black;
		auto white = m_white;

		if (m_state == Clock::State::Running) {
			auto elapsed = std::chrono::duration_cast<Duration>(now - m_lastTick);
			if (elapsed > Duration::zero()) {
				auto& remaining = mainRemainingRef(m_running, black, white);
				remaining       = std::max(remaining - elapsed, Duration::zero());
			}
		}

		return {m_state, m_running, black, white, false, Duration::zero(), 0u};
	}

protected:
	void addMainTime(Player player, Duration delta) {
		if (delta <= Duration::zero()) {
			return;
		}
		mainRemainingRef(player) += delta;
	}

	bool isRunning() const {
		return m_state == Clock::State::Running;
	}

	Duration& mainRemainingRef(Player player) {
		return mainRemainingRef(player, m_black, m_white);
	}

	const Duration& mainRemainingRef(Player player) const {
		return player == Player::Black ? m_black : m_white;
	}

private:
	static Duration& mainRemainingRef(Player player, Duration& black, Duration& white) {
		return player == Player::Black ? black : white;
	}

	void updateElapsed(TimePoint now) {
		auto elapsed = std::chrono::duration_cast<Duration>(now - m_lastTick);
		if (elapsed <= Duration::zero()) {
			m_lastTick = now;
			return;
		}

		auto& remaining = mainRemainingRef(m_running);
		remaining       = std::max(remaining - elapsed, Duration::zero());
		m_lastTick      = now;
	}

private:
	Duration m_black;
	Duration m_white;
};

} // namespace go
