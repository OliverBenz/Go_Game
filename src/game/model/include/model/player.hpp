#pragma once

namespace tengen {

enum class Player { Black = 1, White = 2 };

//! Returns the opponent enum value of input player.
inline constexpr Player opponent(Player player) {
	return player == Player::White ? Player::Black : Player::White;
}

} // namespace tengen
