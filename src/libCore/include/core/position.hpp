#pragma once

#include "core/IZobristHash.hpp"
#include "core/board.hpp"
#include "core/types.hpp"

namespace go {

//! The current game position.
struct Position {
	Board board;                         //!< Current board.
	Player currentPlayer{Player::Black}; //!< Current Player.
	uint64_t hash{0};                    //!< Game state hash.
	unsigned moveId{0};                  //!< Move number of game.

public:
	Position(std::size_t boardSize);

	//! Current player puts a stone.
	//! \note Assumes the move is legal.
	void putStone(Coord c, IZobristHash& hasher);

	//! Current player passes his turn.
	void pass(IZobristHash& hasher);
};

} // namespace go