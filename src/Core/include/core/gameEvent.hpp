#pragma once

#include "data/board.hpp"
#include "data/player.hpp"

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace go {

struct PutStoneEvent {
	Player player;
	Coord c;
};
struct PassEvent {
	Player player;
};
struct ResignEvent {};
struct ShutdownEvent {};
using GameEvent = std::variant<PutStoneEvent, PassEvent, ResignEvent, ShutdownEvent>;


//! Types of signals.
enum GameSignal : std::uint64_t {
	GS_None         = 0,
	GS_BoardChange  = 1 << 0, //!< Board was modified.
	GS_PlayerChange = 1 << 1, //!< Active player changed.
	GS_StateChange  = 1 << 2, //!< Game state changed. Started or finished.
};


//! Type of move.
enum class GameAction { Place, Pass, Resign };

//! Symbolises the game state change after one move.
struct GameDelta {
	unsigned moveId;             //!< Move number.
	GameAction action;           //!< Move type.
	Player player;               //!< Player to make move.
	std::optional<Coord> coord;  //!< For place action: Coordinate of place.
	std::vector<Coord> captures; //!< Captures stones if any.
	Player nextPlayer;           //!< Next player to make a move. In case we add handicap, penalties, etc.
	bool gameActive;             //!< Game active after the move.
};

} // namespace go
