#pragma once

#include "core/types.hpp"
#include <vector>

namespace go {

// TODO: This should be optimized to use 2Bits per site on the field. (B, W, None, ...)
class Board {
public:
	//! Possible ownership values of fields on the board.
	enum class FieldValue {
		None  = 0,
		Black = static_cast<int>(Player::Black),
		White = static_cast<int>(Player::White)
	};

public:
	Board(std::size_t size);

	std::size_t size() const;

	//! With x,y \in [0, size-1]
	//! \note Game coordinates origin is at bottom left of board and start at 0. Column: A->0, B->1, etc
	void setAt(Coord c, FieldValue fieldValue);
	FieldValue getAt(Coord c) const;

private:
	std::size_t m_size;                //!< Board size
	std::vector<FieldValue> m_board{}; //!< Board values.
};

} // namespace go