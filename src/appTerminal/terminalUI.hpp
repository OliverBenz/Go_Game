#pragma once

#incldue "types.hpp"

#include <string>

namespace go::terminal {

//! Draw the board to the terminal
void draw(const Board& board);

//! Try to get a user move from terminal input and return once it's valid.
Coord getMove(const Player player, const Board& board);

}