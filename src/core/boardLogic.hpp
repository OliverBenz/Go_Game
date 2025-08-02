#pragma once

#include <vector>

namespace go {
namespace core{

//! Possible ownership values of fields on the board.
enum class FieldValue : unsigned char {
    None  = 0,
    Black = 1, 
    White = 2
}

// TODO: This should be optimized to use 2Bits per site on the field. (B, W, None, ...)
class BoardLogic {
public:
    BoardLogic(unsigned size);

    //! With x,y \in [1, size]
    //! \note Game coordinates origin is at bottom left of board and start at 1. Column: A->1, B->2, etc
    bool addStone(unsigned x, unsigned y, FieldValue fieldValue);

    FieldValue getAt(unsigned x, unsigned y) const;

private:
    unsigned m_size;                  //!< Board size
    std::vector<FieldValue> m_board;  //!< Board values.
};

}
}