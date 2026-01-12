#pragma once

#include <cstdint>

namespace go {

using Id = unsigned; //!< Board ID used by the core library.

//! Coordinate pair for the board.
struct Coord {
	Id x, y;
};

enum class Player { Black = 1, White = 2 };

//! Returns the opponent enum value of input player.
inline constexpr Player opponent(Player player) {
	return player == Player::White ? Player::Black : Player::White;
}

} // namespace go
