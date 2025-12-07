#pragma once

#include <memory>
#include <set>

namespace go {

class IZobristHash;

class MoveChecker {

private:
    std::unordered_set<int64_t> m_seenHashes; //!< History of board states.
    std::unique_ptr<IZobristHash> m_hasher;   //!< Store the last 2 moves. Allows to check repeating board state.
};

}