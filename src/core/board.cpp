#include "boardLogic.hpp"

namespace go::core {

BoardLogic::BoardLogic(const unsigned size) : m_size(size), m_board(size*size) {
    assert(size == 9 || size == 13 || size == 19);
};

bool BoardLogic::addStone(const Coord c, FieldValue fieldValue) {
    assert(fieldValue != FieldValue::None)
    assert(0 <= c.x && c.x <= m_size-1 && 0 <= c.y && c.y <= m_size-1);

    m_board[c.y * m_size + c.x] = fieldValue;
}

FieldValue BoardLogic::getAt(const Coord c) const {
    assert(0 <= c.x && c.x <= m_size-1 && 0 <= c.y && c.y <= m_size-1);

    return m_board[y * m_size + x];
}

}
