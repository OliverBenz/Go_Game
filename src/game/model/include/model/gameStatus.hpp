#pragma once

namespace tengen {

enum class GameStatus {
	Idle,   //!< Nothing happening.
	Ready,  //!< Players ready to play.
	Active, //!< Game being played.
	Done    //!< Game over.
};

} // namespace tengen
