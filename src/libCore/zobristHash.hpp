#pragma once

#include "core/IZobristHash.hpp"
#include "data/player.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <random>

namespace go {

//! Hash for the current game state. Used to ensure no game state repetition.
template <std::size_t SIZE>
class ZobristHash : public IZobristHash {
public:
	ZobristHash();

	uint64_t stone(Coord c, Player color) override; //!< Update on placing a stone.
	uint64_t togglePlayer() override;               //!< Update for player-to-move swap (needed for situational superko).

private:
	void initRandomTable();

private:
	std::array<std::array<std::array<uint64_t, 2>, SIZE>, SIZE> m_table{};
	uint64_t m_playerToggle{0}; //!< Hash for player toggle.
};

template <std::size_t SIZE>
ZobristHash<SIZE>::ZobristHash() {
	initRandomTable();
}

template <std::size_t SIZE>
uint64_t ZobristHash<SIZE>::stone(Coord c, Player color) {
	assert(static_cast<int>(color) == 1 || static_cast<int>(color) == 2);
	assert(c.x < SIZE && c.y < SIZE);

	return m_table[c.x][c.y][static_cast<unsigned>(color) - 1u];
}

template <std::size_t SIZE>
uint64_t ZobristHash<SIZE>::togglePlayer() {
	return m_playerToggle;
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::initRandomTable() {
	std::mt19937_64 rng(0xA5F3C7E2B1D94ULL); //!< Fixed seed for reproducibility
	std::uniform_int_distribution<uint64_t> dist;

	for (std::size_t x = 0; x < SIZE; ++x)
		for (std::size_t y = 0; y < SIZE; ++y)
			for (std::size_t c = 0; c < 2; ++c)
				m_table[x][y][c] = dist(rng);

	m_playerToggle = dist(rng);
}
} // namespace go
