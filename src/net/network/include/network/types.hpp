#pragma once

#include <cstdint>

namespace go::network {

using SessionId = std::uint32_t;

enum class ServerAction : std::uint8_t {
	Place,
	Pass,
	Resign,
	Count //!< Used in serialisation to check when enum changes.
};

enum class GameStatus : std::uint8_t {
	Active,
	BlackWin,
	WhiteWin,
	Draw,
	Count //!< Used in serialisation to check when enum changes.
};

//! The role in the game.
enum class Seat : std::uint8_t {
	None     = 0,      //!< Just connected.
	Black    = 1 << 1, //!< Plays for black.
	White    = 1 << 2, //!< Plays for white.
	Observer = 1 << 3  //!< Only gets updated on board change.
};

inline constexpr bool isPlayer(Seat seat) {
	return seat == Seat::Black || seat == Seat::White;
}

} // namespace go::gameNet
