#pragma once

#include "core/types.hpp"
#include <vector>

namespace go {

// TODO: This should be optimized to use 2Bits per site on the field. (B, W, None, ...)
//! \note Game coordinates origin is at bottom left of board and start at 0. Column: A->0, B->1, etc
class Board {
public:
	//! Possible ownership values of fields on the board.
	enum class Value { Empty = 0, Black = static_cast<int>(Player::Black), White = static_cast<int>(Player::White) };

public:
	Board(std::size_t size);

	std::size_t size() const;

	void setAt(Coord c, Value value); //!< Set at given coordinate (x,y) \in [0, size-1]
	Value getAt(Coord c) const;       //!< Get value at given coordinate (x,y) \in [0, size-1]
	bool isFree(Coord c) const;       //!< Returns whether a certain board coordinate is free or occupied.

private:
	std::size_t m_size;           //!< Board size
	std::vector<Value> m_board{}; //!< Board values.
};

//! Returns the Board::Value enum value of input player.
inline constexpr Board::Value toBoardValue(Player player) {
	return player == Player::White ? Board::Value::White : Board::Value::Black;
}

} // namespace go
