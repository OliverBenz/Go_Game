#include "boardLogic.hpp"

namespace go {
namespace core {

BoardLogic::BoardLogic(unsigned size) : m_size(size), m_board(size*size)
{};

bool BoardLogic::addStone(unsigned x, unsigned y, FieldValue fieldValue) {
    assert(fieldValue != FieldValue::None)
    assert(1 <= x && x <= m_size && 1 <= y && y <= m_size);

    m_board[(y-1) * m_size + (x-1)] = fieldValue;
}

FieldValue BoardLogic::getAt(unsigned x, unsigned y) const {
    assert(1 <= x && x <= m_size && 1 <= y && y <= m_size);

    return m_board[(y-1) * m_size + (x-1)];
}

}
}
