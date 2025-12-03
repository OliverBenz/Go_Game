#include "terminalUI.hpp"

#include <iostream>

namespace go::app {

//! Alphanumeric position to board index.
static bool moveToId(std::string& move, Coord& id) {
	// TODO: SOlidify, this is bug prone

	const int horizontal = std::tolower(move[0]) - 97;   // Lowercase ascii letter to int
	if(horizontal > size-1 || horizontal < 0) {
		return false;
	}
	
	try{
		const int vertical = size - std::stoi(move.substr(1)); // User inputs 1-size
		if(vertical > size || vertical < 0) {
			return false;
		}

		id = {horizontal, vertical};
	} catch (...) {
		return false;
	}
	
	return true;
}

//! Get user input from terminal.
static Coord input(const Player player) {
	std::string moveStr;

	std::cout << (player == Player::Black ? "Black Move: " : "White Move: ");
	std::cin >> moveStr;

	Coord moveId = 0;
	if (!moveToId(moveStr, moveId)) {
		return input(player);
	}
	return moveId;
}

// TODO: Check move valid. Needs board
static bool isValidMove(const Player player, const Coord& moveId, const Board& board) {
	if (moveId >= size*size) {
		return false;
	}

	// board position free, etc...

	return true;
}

void draw(const Board& board) {
	std::cout << "\033[2J\033[1;1H" << std::flush;

	std::cout << "   ";
	for (std::size_t i = 0; i != size; ++i) {
		std::cout << static_cast<char>(97+i) << " ";
	}
	std::cout << "\n";

	for (std::size_t i = 0; i != size*size; ++i) {
		if (i % size == 0) {
			std::cout << "\n" << (size - (i / size)) << "  ";
		}

		char symb = ' ';
		switch (board[i]) {
			case 1:
				symb = 'x';
				break;
			case -1:
				symb = 'o';
				break;
		}
		std::cout << symb << " ";
	}
	std::cout << std::endl;
}

Coord getMove(const Player player, const Board& board) {
	Coord moveId = 0;
	do {
		moveId = input(player);
	} while(!isValidMove(player, moveId, board));

	return moveId;
}

}