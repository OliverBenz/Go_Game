#include "terminalUI.hpp"

#include "core/game.hpp"

#include <iostream>
#include <thread>

namespace go::terminal {

// TODO: Remove core namespace
// TODO: Write cmake
// TODO: Get running
// TODO: Includes subdirectory for libs

void play() {
	static constexpr std::size_t BOARD_SIZE = 9; 

	Game game;
	
	game.setup(BOARD_SIZE);

	std::thread gameThread([&]{
		game.run();
	});

	while (true) {
		draw(game.board());

		const auto move = getMove(game.currentPlayer(), game.board());
		// TODO: Game core should also verify isValidMove
		game.pushEvent(PutStoneEvent{move});
	}
}

void review() {
	std::cout << "Not yet implemented.\n";
}

}

int main() {
	char option = ' ';
	std::cout << "Play (P) or Review (R): ";
	std::cin >> option;
	
	switch (option) {
		case 'p':
		case 'P':
			go::terminal::play();
			break;
		case 'r':
		case 'R':
			go::terminal::review();
			break;
		default:
			std::cout << "Invalid selection." << std::endl;
	}
}
