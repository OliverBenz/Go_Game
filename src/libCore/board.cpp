#include "core/board.hpp"

#include <cassert>

namespace go {

Board::Board(const std::size_t size) : m_size(size), m_board(size * size, Value::Empty) {
	assert(size == 9 || size == 13 || size == 19); // Core only supports standard board sizes; invalid sizes are a programmer error.
};

std::size_t Board::size() const {
	return m_size;
}

void Board::setAt(const Coord c, Value value) {
	assert(c.x < m_size && c.y < m_size); // Game should heck isValidMove trying to set

	m_board[c.y * m_size + c.x] = value;
}

Board::Value Board::getAt(const Coord c) const {
	assert(c.x < m_size && c.y < m_size);

	return m_board[c.y * m_size + c.x];
}

bool Board::isFree(const Coord c) const {
	return getAt(c) == Value::Empty;
}

} // namespace go
