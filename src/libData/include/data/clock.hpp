#pragma once

#include "data/player.hpp"
#include "data/clockHandler.hpp"

#include <chrono>
#include <memory>
#include <variant>

namespace go {

// TODO: Match state and clock belong to core library. NOT data layer.

class Clock {
public:
	using Duration  = std::chrono::milliseconds;
	using TimePoint = std::chrono::steady_clock::time_point;

	enum class State { Stopped, Running, Paused };

	// Clock configurations
	struct Absolute {
		Duration mainTime;
	};
	struct Fischer {
		Duration mainTime;
		Duration increment;
	};
	struct ByoYomi {
		Duration mainTime;
		Duration period;
		unsigned periods;
	};
	using Config = std::variant<Absolute, Fischer, ByoYomi>;

	struct Snapshot {
		State state;
		Player running;

		Duration blackMain;
		Duration whiteMain;

		bool inOvertime; //!< Overtime info for the running player.
		Duration overtimeRemaining;
		unsigned overtimePeriodsRemaining;
	};

public:
	explicit Clock(const Config& config);
	~Clock();

	void start(Player toMove);
	void push(Player whoMoved);

	void pause();
	void resume();
	void stop();
    void reset();

	Snapshot snapshot() const;

private:
	std::unique_ptr<ClockHandler> m_handler;
};

} // namespace go
