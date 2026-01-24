#pragma once

#include "data/board.hpp"
#include "data/player.hpp"

namespace go {

enum class GameStatus {
	Idle,   //!< Nothing happening.
	Ready,  //!< Players ready to play.
	Active, //!< Game being played.
	Done    //!< Game over.
};

//! Current game position.
struct Position {
	Player toMove; //!< Current player to make a move.
	Board board;   //!< Go Board.
};

} // namespace go
