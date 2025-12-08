#include "core/game.hpp"
#include "gameWindow.hpp"

#include <thread>

int main() {
	static constexpr std::size_t boardSize = 9u;
	go::Game game(boardSize);
	std::thread gameThread([&] { game.run(); });

	go::sdl::GameWindow window(800, 800, game);

	window.run();

	gameThread.join();
}