#pragma once

#include "core/IZobristHash.hpp"
#include "model/board.hpp"
#include "model/player.hpp"

namespace tengen {

//! The current game position.
struct GamePosition {
	Board board;                         //!< Current board.
	Player currentPlayer{Player::Black}; //!< Current Player.
	uint64_t hash{0};                    //!< Game state hash.
	unsigned moveId{0};                  //!< Move number of game.

public:
	GamePosition(std::size_t boardSize);

	void putStone(Coord c, IZobristHash& hasher); //!< Current player puts a stone (assumes legal move).
	void pass(IZobristHash& hasher);              //!< Current player passes his turn.
};

} // namespace tengen
