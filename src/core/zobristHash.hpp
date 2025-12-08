#pragma once

#include "core/types.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <random>

namespace go {

//! Interface to allow storing different board size instantiations.
class IZobristHash {
public:
	virtual uint64_t lookAhead(Coord c, Player color) const = 0;
	virtual void placeStone(Coord c, Player color)          = 0;
	virtual void removeStone(Coord c, Player color)         = 0;
	virtual void togglePlayer()                             = 0;

	virtual void reset()           = 0;
	virtual uint64_t value() const = 0;
};


//! Hash for the current game state. Used to ensure no game state repetition.
template <std::size_t SIZE>
class ZobristHash : public IZobristHash {
public:
	ZobristHash();

	//! Reset hash.
	void reset() override;

	//! Get current hash.
	uint64_t value() const override;

	//! Returns the hash that would happen after a move.
	uint64_t lookAhead(Coord c, Player color) const override;

	//! Update on placing a stone.
	void placeStone(Coord c, Player color) override;

	//! Update on removing a stone.
	void removeStone(Coord c, Player color) override;

	// Update for player-to-move swap (needed for situational superko).
	void togglePlayer() override;

private:
	void initRandomTable();

private:
	uint64_t m_hash = 0;       //!< Game state hash
	uint64_t m_playerToggle{}; //!< Hash for player toggle.
	std::array<std::array<std::array<uint64_t, 2>, SIZE>, SIZE> m_table{};
};

template <std::size_t SIZE>
ZobristHash<SIZE>::ZobristHash() {
	initRandomTable();
	reset();
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::reset() {
	m_hash = 0;
}

template <std::size_t SIZE>
uint64_t ZobristHash<SIZE>::value() const {
	return m_hash;
}

template <std::size_t SIZE>
uint64_t ZobristHash<SIZE>::lookAhead(Coord c, Player color) const {
	assert(static_cast<int>(color) == 1 || static_cast<int>(color) == 2);
	assert(c.x < SIZE && c.y < SIZE);

	return m_hash ^ m_table[c.x][c.y][static_cast<unsigned>(color) - 1u];
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::placeStone(Coord c, Player color) {
	assert(static_cast<int>(color) == 1 || static_cast<int>(color) == 2);
	assert(c.x < SIZE && c.y < SIZE);

	m_hash ^= m_table[c.x][c.y][static_cast<unsigned>(color) - 1u];
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::removeStone(Coord c, Player color) {
	assert(static_cast<int>(color) == 1 || static_cast<int>(color) == 2);
	assert(c.x < SIZE && c.y < SIZE);
	m_hash ^= m_table[c.x][c.y][static_cast<unsigned>(color) - 1u];
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::togglePlayer() {
	m_hash ^= m_playerToggle;
}

template <std::size_t SIZE>
void ZobristHash<SIZE>::initRandomTable() {
	std::mt19937_64 rng(0xA5F3C7E2B1D94ULL); //!< Fixed seed for reproducibility
	std::uniform_int_distribution<uint64_t> dist;

	for (int x = 0; x < SIZE; ++x)
		for (int y = 0; y < SIZE; ++y)
			for (int c = 0; c < 2; ++c)
				m_table[x][y][c] = dist(rng);

	m_playerToggle = dist(rng);
}
} // namespace go
