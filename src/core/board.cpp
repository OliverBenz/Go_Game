#include "core/board.hpp"

#include <cassert>

namespace go {

Board::Board(const std::size_t size) : m_size(size), m_board(size * size, Value::Empty) {
	assert(size == 9 || size == 13 || size == 19);
};

std::size_t Board::size() const {
	return m_size;
}

void Board::setAt(const Coord c, Value Value) {
	assert(Value != Board::Value::Empty);
	assert(0 <= c.x && c.x <= m_size - 1 && 0 <= c.y &&
	       c.y <= m_size - 1); // Game should heck isValidMove trying to set

	m_board[c.y * m_size + c.x] = Value;
}

Board::Value Board::getAt(const Coord c) const {
	assert(0 <= c.x && c.x <= m_size - 1 && 0 <= c.y && c.y <= m_size - 1);

	return m_board[c.y * m_size + c.x];
}

bool Board::isFree(const Coord c) const {
	return getAt(c) == Value::Empty;
}

} // namespace go
