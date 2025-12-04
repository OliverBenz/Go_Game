#include "window.hpp"

int main() {
    go::Window window(800, 800);
    go::Game game;

    game.setup(9u);
	std::thread gameThread([&]{
		game.run();
	});

    window.run(game);
    gameThread.join();
}