#pragma once

#include "core/types.hpp"

#include <variant>

namespace go {

struct PutStoneEvent {
	Player player;
	Coord c;
};
struct PassEvent {
	Player player;
};
struct ResignEvent {};
struct ShutdownEvent {};

using GameEvent = std::variant<PutStoneEvent, PassEvent, ResignEvent, ShutdownEvent>;

} // namespace go
