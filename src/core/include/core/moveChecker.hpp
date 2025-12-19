#pragma once

#include "core/board.hpp"
#include "core/types.hpp"

#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

namespace go {

class IZobristHash;
struct Position;

//! Returns the liberties of the group connected to startCoord if player placed a stone there.
std::size_t computeGroupLiberties(const Board& board, Coord startCoord, Player player);

//! Check whether a move would be suicidal (ignoring superko).
bool isSuicide(const Board& board, Player player, Coord c);

//! Full legality check (bounds, occupancy, suicide) without superko.
bool isValidMove(const Board& board, Player player, Coord c);

//! Compute resulting position if the move is legal (including superko via history). Returns false when illegal.
bool isNextPositionLegal(const Position& current, Player player, Coord c, IZobristHash& hasher,
                         const std::unordered_set<uint64_t>& history, Position& out);

} // namespace go
