#include "core/board.hpp"

#include <cassert>

namespace go {

Board::Board(const std::size_t size) : m_size(size), m_board(size * size, Value::Empty) {
	assert(size == 9 || size == 13 || size == 19);
};

std::size_t Board::size() const {
	return m_size;
}

void Board::setAt(const Coord c, Value value) {
	assert(value != Board::Value::Empty);
	assert(c.x < m_size && c.y < m_size); // Game should heck isValidMove trying to set

	m_board[c.y * m_size + c.x] = value;
}

Board::Value Board::getAt(const Coord c) const {
	assert(c.x < m_size && c.y < m_size);

	return m_board[c.y * m_size + c.x];
}
void Board::remAt(const Coord c) {
	assert(c.x < m_size && c.y < m_size);

	m_board[c.y * m_size + c.x] = Board::Value::Empty;
}

bool Board::isFree(const Coord c) const {
	return getAt(c) == Value::Empty;
}

} // namespace go
