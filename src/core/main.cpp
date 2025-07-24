#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

constexpr std::size_t size = 9;


//! Alphanumeric position to board index.
bool moveToId(std::string& move, std::size_t& id) {
	// TODO: SOlidify, this is bug prone

	const int horizontal = std::tolower(move[0]) - 97;   // Lowercase ascii letter to int
	if(horizontal > size-1 || horizontal < 0) {
		return false;
	}
	
	try{
		const int vertical = size - std::stoi(move.substr(1)); // User inputs 1-size
		if(vertical > size || vertical < 1) {
			return false;
		}

		id = static_cast<std::size_t>(vertical * size + horizontal);
	} catch (...) {
		return false;
	}
	
	return true;
}


//! Get user input from terminal.
std::size_t input(const bool turnBlack) {
	std::string moveStr;

	std::cout << (turnBlack ? "Black Move: " : "White Move: ");
	std::cin >> moveStr;

	std::size_t moveId = 0;
	if (!moveToId(moveStr, moveId)) {
		return input(turnBlack);
	}
	return moveId;
}

// TODO: Check move valid. Needs board
bool isValidMove(const bool turnBlack, const std::size_t moveId, const std::array<int, size*size>& board) {
	if (moveId >= size*size - 1) {
		return false;
	}

	// board position free, etc...

	return true;
}

//! Try to get a user move and return once it's valid.
std::size_t getMove(const bool turnBlack, const std::array<int, size*size>& board) {
	std::size_t moveId = 0;
	do {
		moveId = input(turnBlack);
	} while(!isValidMove(turnBlack, moveId, board));

	return moveId;
}




void drawBoard(const std::array<int, size*size>& board){
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



void play() {
	std::array<int, size*size> board{};

	bool end = false;
	bool turnBlack = true;
	drawBoard(board);
	while(!end) {
		const auto moveId = getMove(turnBlack, board);

		board[moveId] = (turnBlack ? 1 : -1);
		drawBoard(board);
		assert(moveId <= size*size);
		turnBlack = !turnBlack;
	}
}

void review() {
	std::cout << "Not yet implemented.\n";
}

int main() {
	char option = ' ';
	std::cout << "Play (P) or Review (R): ";
	std::cin >> option;
	
	switch (option) {
		case 'p':
		case 'P':
			play();
			break;
		case 'r':
		case 'R':
			review();
			break;
		default:
			std::cout << "Invalid selection." << std::endl;
	}
}
