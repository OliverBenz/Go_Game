#include "core/game.hpp"

#include "GameServer.hpp"

int main(int argc, char** argv) {
	static constexpr std::size_t boardSize = 9u;

	go::Game game(boardSize);
	std::thread gameThread([&] { game.run(); });

	GameServer server(game);

	return 0;
}