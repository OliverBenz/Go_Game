#pragma once

#include "data/coordinate.hpp"
#include "data/player.hpp"

#include <cassert>
#include <vector>

namespace go {

//! A physical go board of arbitrary size.
class Board {
public:
	enum class Stone { Empty, Black, White };

	Board(std::size_t size);

	bool place(Coord c, Stone value); //!< Try to place a stone at the given coordinate. False if not free.
	bool remove(Coord c);             //!< Remove the stone at the given coordinate. False if already free.

	Stone get(Coord c) const;    //!< Get the stone at the given position.
	bool isEmpty(Coord c) const; //!< True if the given coordinate is empty.
	std::size_t size() const;    //!< Size of the board.

private:
	std::size_t m_size{0u};       //!< Board size (typically 9, 13, 19)
	std::vector<Stone> m_board{}; //!< Board data.
};

//! Maps a player color to a stone color.
inline constexpr Board::Stone toStone(const Player player) {
	assert(player == Player::Black || player == Player::White);
	return player == Player::White ? Board::Stone::White : Board::Stone::Black;
}

} // namespace go