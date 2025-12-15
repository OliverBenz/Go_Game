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

	//! Returns the liberties of a group connected to startCoord of given player together with the coordinates of each group element.
	std::size_t groupAnalysis(const Coord& startCoord, const Player player, std::vector<Coord>& group);

	//! Get coordinates of all stones in a group if it has been killed by a move.
	// \param [in] startCoord Start coordinate of the group to check if its alive.
	// \param [in] player     Player color of group to find.
	// \note This function assumes the opposite player placed a stone at a neighbouring field but the board has not yet been updated.
	std::vector<Coord> getKilledByMove(const Coord& startCoord, const Player player);

	//! Compute the gamestate hash after a move.
	uint64_t hashAfterMove(Player player, Coord c);

private:
	const Board& m_board;                      //!< Reference the game board.
	std::unordered_set<uint64_t> m_seenHashes; //!< History of board states.
	std::unique_ptr<IZobristHash> m_hasher;    //!< Store the last 2 moves. Allows to check repeating board state.
};

} // namespace go
