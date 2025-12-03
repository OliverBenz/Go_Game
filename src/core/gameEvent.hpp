#pragma once

namespace go::core {

struct PutStoneEvent { Coord c; Player player; };
struct PassEvent { Player player; };
struct ResignEvent { Player player; };
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
