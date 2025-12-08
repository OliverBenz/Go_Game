#pragma once

#include "core/types.hpp"

#include <variant>

namespace go {

struct PutStoneEvent {
	Coord c;
};
struct PassEvent {};
struct ResignEvent {};
struct ShutdownEvent {};

using GameEvent = std::variant<PutStoneEvent, PassEvent, ResignEvent, ShutdownEvent>;

} // namespace go
