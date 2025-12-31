#include "core/game.hpp"

#include "GameServer.hpp"

#include <thread>

int main(int, char**) {
	static constexpr std::size_t boardSize = 9u;

	go::Game game(boardSize);
	std::thread gameThread([&] { game.run(); });

	go::server::GameServer server(game);
	server.start(); // Uncomment when running the server as a standalone process.

	if (gameThread.joinable()) {
		gameThread.join();
	}

	return 0;
}
