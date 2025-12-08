#pragma once

#include "core/board.hpp"
#include "core/types.hpp"

#include <memory>
#include <unordered_set>

namespace go {

class IZobristHash;

class MoveChecker {
public:
	MoveChecker(const Board& board);
	~MoveChecker();

	//! Check if a move is valid.
	bool isValidMove(Player player, Coord c);

private:
	//! Compute the gamestate hash after a move.
	uint64_t hashAfterMove(Player player, Coord c);

private:
	const Board& m_board;                      //!< Reference the game board.
	std::unordered_set<uint64_t> m_seenHashes; //!< History of board states.
	std::unique_ptr<IZobristHash> m_hasher;    //!< Store the last 2 moves. Allows to check repeating board state.
};

} // namespace go