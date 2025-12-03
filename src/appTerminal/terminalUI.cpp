#include "terminalUI.hpp"

#include "core/types.hpp"

#include <iostream>

namespace go::terminal {

//! Alphanumeric position to board index.
static bool moveToId(std::string& move, Coord& id) {
	// TODO: SOlidify, this is bug prone

    // TODO: in verify method
	const int horizontal = std::tolower(move[0]) - 97;   // Lowercase ascii letter to int
	if(/*horizontal > size-1 || */ horizontal < 0) {
		return false;
	}
	
	try{
        // TODO: in verify method
		const int vertical = std::stoi(move.substr(1)); // User inputs 1-size
		if(/*vertical > size || */ vertical < 0) {
			return false;
		}

		id = {static_cast<Id>(horizontal), static_cast<Id>(vertical)};
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

	Coord moveId;
	if (!moveToId(moveStr, moveId)) {
		return input(player);
	}
	return moveId;
}

// TODO: Check move valid. Needs board
static bool isValidMove(const Player player, const Coord& moveId, const Board& board) {
	// if (moveId >= board.size()*board.size()) {
	// 	return false;
	// }

	// board position free, etc...

	return true;
}

void draw(const Board& board) {
    const auto BOARD_SIZE = board.size();

	std::cout << "\033[2J\033[1;1H" << std::flush;

	std::cout << "   ";
	for (std::size_t i = 0; i != BOARD_SIZE; ++i) {
		std::cout << static_cast<char>(97+i) << " ";
	}
	std::cout << "\n";

	for (Id i = 0; i != BOARD_SIZE; ++i) {
        for(Id j = 0; i != BOARD_SIZE; ++j) {
		    char symb = ' ';
		    switch (board.getAt(Coord{i, j})) {
		    	case Board::FieldValue::Black:
		    		symb = 'x';
		    		break;
		    	case Board::FieldValue::White:
		    		symb = 'o';
		    		break;
		    }
		    std::cout << symb << " ";
        }
		std::cout << "\n" << (BOARD_SIZE - i) << "  ";
	}
	std::cout << std::endl;
}

Coord getMove(const Player player, const Board& board) {
	Coord moveId;
	do {
		moveId = input(player);
	} while(!isValidMove(player, moveId, board));

	return moveId;
}

}