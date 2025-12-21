#pragma once

#include <cstdint>

namespace go {

using Id = unsigned; //!< Board ID used by the core library.

//! Coordinate pair for the board.
struct Coord {
	Id x, y;
};

enum class Player { Black = 1, White = 2 };

//! Types of notifications.
enum GameSignal : uint64_t {
	GS_None         = 0,
	GS_BoardChange  = 1 << 0, //!< Board was modified.
	GS_PlayerChange = 1 << 1, //!< Active player changed.
	GS_StateChange  = 1 << 2, //!< Game state changed. Started or finished.
};


//! Returns the opponent enum value of input player.
inline constexpr Player opponent(Player player) {
	return player == Player::White ? Player::Black : Player::White;
}

} // namespace go
