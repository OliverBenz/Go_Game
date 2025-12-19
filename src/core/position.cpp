#include "core/position.hpp"

namespace go {

Position::Position(std::size_t boardSize) : board{boardSize} {
}

void Position::putStone(Coord c, IZobristHash& hasher) {
	board.setAt(c, toBoardValue(currentPlayer));
	hash ^= hasher.stone(c, currentPlayer);

	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();
}

void Position::pass(IZobristHash& hasher) {
	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();
}

} // namespace go