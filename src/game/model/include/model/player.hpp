#pragma once

#include <cassert>
#include <cstdint>
#include <string>

namespace tengen {

// Player Info
enum class Player : int8_t {
	Black = 1,
	White = 2,
};
inline constexpr Player opponent(Player player);  //!< Returns the opponent enum value of input player.
inline std::string toString(const Player player); //!< Convert a player to a string as "White" and "Black".

// Skill Level
enum class Skill : int8_t {}; //!< Strength increasing. So Skill{0}=30kyu -> Skill{29}=1kyu -> Skill{30}=1dan -> Skill{38}=9dan

inline constexpr Skill fromKyu(int k) noexcept;           //!< Convert a numeric kyu skill level to our 'Skill' convention.
inline constexpr Skill fromDan(int d) noexcept;           //!< Convert a numeric dan skill level to our 'Skill' convention.
inline constexpr int stoneGap(Skill a, Skill b) noexcept; //!< Roughly how many stones separate two players/bots.
inline std::string toString(Skill s);                     //!< Convert a skill level to a string (e.g. 30k, 9d).

//
// Implementations
//
inline constexpr Player opponent(const Player player) {
	return player == Player::White ? Player::Black : Player::White;
}
inline std::string toString(const Player player) {
	return player == Player::White ? "White" : "Black";
}

inline constexpr int8_t danOffset = 30;
inline constexpr Skill fromKyu(const int kyu) noexcept {
	assert(kyu >= 1 && kyu <= 30);
	return Skill{static_cast<int8_t>(danOffset - kyu)};
}
inline constexpr Skill fromDan(const int dan) noexcept {
	assert(dan >= 1 && dan <= 9);
	return Skill{static_cast<int8_t>(danOffset - 1 + dan)};
}
inline constexpr int stoneGap(const Skill playerA, const Skill playerB) noexcept {
	const int diff = static_cast<int8_t>(playerA) - static_cast<int8_t>(playerB);
	return diff < 0 ? -diff : diff;
}
inline std::string toString(const Skill s) {
	const auto v = static_cast<int8_t>(s);
	return v < danOffset
	               ? std::to_string(danOffset - v) + "k"
	               : std::to_string(v - danOffset + 1) + "d";
}

} // namespace tengen
