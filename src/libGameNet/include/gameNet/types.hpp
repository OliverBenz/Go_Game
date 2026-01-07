#pragma once

namespace go::gameNet {

//! The role in the game.
enum class Seat : std::uint8_t {
	None     = 0,      //!< Just connected.
	Black    = 1 << 1, //!< Plays for black.
	White    = 1 << 2, //!< Plays for white.
	Observer = 1 << 3  //!< Only gets updated on board change.
};

} // namespace go::gameNet