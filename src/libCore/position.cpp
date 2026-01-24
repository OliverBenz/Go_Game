#include "core/position.hpp"

namespace go {

Position::Position(std::size_t boardSize) : board{boardSize} {
}

void Position::putStone(Coord c, IZobristHash& hasher) {
	board.place(c, toStone(currentPlayer));
	hash ^= hasher.stone(c, currentPlayer);

	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();

	++moveId;
}

void Position::pass(IZobristHash& hasher) {
	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();

	++moveId;
}

} // namespace go