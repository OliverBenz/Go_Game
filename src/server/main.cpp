#include "core/game.hpp"

#include "GameServer.hpp"

int main(int, char**) {
	static constexpr std::size_t boardSize = 9u;

	go::Game game(boardSize);
	std::thread gameThread([&] { game.run(); });

	go::server::GameServer server(game);

	return 0;
}