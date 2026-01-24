#pragma once

#include "ClockHandler.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

namespace go {

class ByoYomiClock final : public ClockHandler {
public:
	struct Side {
		Duration main;
		Duration overtime;
		unsigned periods;
	};

	ByoYomiClock(Duration main, Duration period, unsigned periods)
	    : m_period(std::max(period, Duration::zero())), m_periods(periods), m_black{std::max(main, Duration::zero()), Duration::zero(), 0u},
	      m_white{std::max(main, Duration::zero()), Duration::zero(), 0u} {
		if (hasOvertime()) {
			m_black.overtime = m_period;
			m_white.overtime = m_period;
			m_black.periods  = periods;
			m_white.periods  = periods;
		}
	}

	void push(Player whoMoved, TimePoint now) override {
		if (m_state != Clock::State::Running) {
			return;
		}

		updateElapsed(now);
		assert(m_running == whoMoved);

		auto& side = sideRef(whoMoved);
		if (isInOvertime(side) && side.periods > 0u) {
			side.overtime = m_period;
		}

		m_running  = opponent(whoMoved);
		m_lastTick = now;
	}

	Clock::Snapshot snapshot(TimePoint now) const override {
		auto black = m_black;
		auto white = m_white;

		if (m_state == Clock::State::Running) {
			auto elapsed = std::chrono::duration_cast<Duration>(now - m_lastTick);
			if (elapsed > Duration::zero()) {
				applyElapsed(elapsed, sideRef(m_running, black, white));
			}
		}

		const auto& running   = sideRef(m_running, black, white);
		const bool inOvertime = isInOvertime(running);

		return {m_state, m_running, black.main, white.main, inOvertime, inOvertime ? running.overtime : Duration::zero(), inOvertime ? running.periods : 0u};
	}

private:
	bool hasOvertime() const {
		return m_period > Duration::zero() && m_periods > 0u;
	}

	bool isInOvertime(const Side& side) const {
		return hasOvertime() && side.main == Duration::zero();
	}

	Side& sideRef(Player player) {
		return player == Player::Black ? m_black : m_white;
	}

	static Side& sideRef(Player player, Side& black, Side& white) {
		return player == Player::Black ? black : white;
	}

	void updateElapsed(TimePoint now) {
		auto elapsed = std::chrono::duration_cast<Duration>(now - m_lastTick);
		if (elapsed <= Duration::zero()) {
			m_lastTick = now;
			return;
		}

		applyElapsed(elapsed, sideRef(m_running));
		m_lastTick = now;
	}

	void applyElapsed(Duration elapsed, Side& side) const {
		if (elapsed <= Duration::zero()) {
			return;
		}

		const auto mainUsed = std::min(elapsed, side.main);
		side.main -= mainUsed;
		elapsed -= mainUsed;
		if (elapsed == Duration::zero()) {
			return;
		}

		if (!hasOvertime() || side.periods == 0u) {
			side.overtime = Duration::zero();
			side.periods  = 0u;
			return;
		}

		side.overtime           = std::max(side.overtime, m_period);
		const auto overtimeUsed = std::min(elapsed, side.overtime);
		side.overtime -= overtimeUsed;
		elapsed -= overtimeUsed;
		if (elapsed == Duration::zero()) {
			return;
		}

		const auto fullPeriods         = static_cast<unsigned>(elapsed / m_period);
		const unsigned periodsConsumed = fullPeriods + 1u;
		if (periodsConsumed >= side.periods) {
			side.periods  = 0u;
			side.overtime = Duration::zero();
			return;
		}

		side.periods -= periodsConsumed;
		const auto remainder = elapsed % m_period;
		side.overtime        = (remainder == Duration::zero()) ? m_period : (m_period - remainder);
	}

private:
	Duration m_period;
	unsigned m_periods;

	Side m_black;
	Side m_white;
};

} // namespace go
