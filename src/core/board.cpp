#include "core/board.hpp"

#include <cassert>

namespace go {

Board::Board(const std::size_t size) : m_size(size), m_board(size*size, FieldValue::None) {
    assert(size == 9 || size == 13 || size == 19);
};

std::size_t Board::size() const {
    return m_size;
}

void Board::setAt(const Coord c, FieldValue fieldValue) {
    assert(fieldValue != Board::FieldValue::None);
    assert(0 <= c.x && c.x <= m_size-1 && 0 <= c.y && c.y <= m_size-1); // Game should heck isValidMove trying to set

    m_board[c.y * m_size + c.x] = fieldValue;
}

Board::FieldValue Board::getAt(const Coord c) const {
    assert(0 <= c.x && c.x <= m_size-1 && 0 <= c.y && c.y <= m_size-1);

    return m_board[c.y * m_size + c.x];
}

}
