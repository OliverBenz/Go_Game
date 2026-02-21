#include "core/position.hpp"

namespace tengen {

GamePosition::GamePosition(std::size_t boardSize) : board{boardSize} {
}

void GamePosition::putStone(Coord c, IZobristHash& hasher) {
	board.place(c, toStone(currentPlayer));
	hash ^= hasher.stone(c, currentPlayer);

	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();

	++moveId;
}

void GamePosition::pass(IZobristHash& hasher) {
	currentPlayer = opponent(currentPlayer);
	hash ^= hasher.togglePlayer();

	++moveId;
}

} // namespace tengen