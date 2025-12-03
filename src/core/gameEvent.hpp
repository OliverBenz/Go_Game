#pragma once

#include "types.hpp"

#include <variant>

namespace go::core {

struct PutStoneEvent { Coord c; };
struct PassEvent { };
struct ResignEvent { };
struct NetworkMoveEvent { Coord c; };
struct ShutdownEvent {};

using GameEvent = std::variant<
    PutStoneEvent,
    PassEvent,
    ResignEvent,
    NetworkMoveEvent,
    ShutdownEvent
>;

}
