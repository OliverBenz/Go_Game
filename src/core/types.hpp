#pragma once

namespace go::core {

using Id = unsigned;  //!< Board ID used by the core library.

//! Coordinate pair for the board.
struct Coord {
    Id x, y;
};

enum class Player {
    Black = 1,
    White = 2
};

}
