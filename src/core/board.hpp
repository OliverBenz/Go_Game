#pragma once

#include "types.hpp"
#include <vector>

namespace go::core {

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

    //! With x,y \in [0, size-1]
    //! \note Game coordinates origin is at bottom left of board and start at 0. Column: A->0, B->1, etc
    bool setAt(Coord c, FieldValue fieldValue);
    FieldValue getAt(Coord c) const;

private:
    std::size_t m_size;               //!< Board size
    std::vector<FieldValue> m_board;  //!< Board values.
};

}