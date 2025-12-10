#pragma once

#include "core/board.hpp"
#include "core/types.hpp"

#include <memory>
#include <unordered_set>

namespace go {

class IZobristHash;

// TODO: findGroupAndLiberties ()
class MoveChecker {
public:
	MoveChecker(const Board& board);
	~MoveChecker();

	//! Check if a move is valid.
	bool isValidMove(Player player, Coord c);

	//! Compute the liberties of the connected group when Player places a stone at startCoord.
	std::size_t computeGroupLiberties(const Coord& startCoord, const Player player);

private:
	//! Check if the move is a suicide move.
	bool isSuicide(Player player, Coord c);

	//! Compute the gamestate hash after a move.
	uint64_t hashAfterMove(Player player, Coord c);

private:
	const Board& m_board;                      //!< Reference the game board.
	std::unordered_set<uint64_t> m_seenHashes; //!< History of board states.
	std::unique_ptr<IZobristHash> m_hasher;    //!< Store the last 2 moves. Allows to check repeating board state.
};

} // namespace go