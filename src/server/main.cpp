#include "core/game.hpp"

#include "gameServer.hpp"

#include <thread>

int main(int, char**) {
	go::server::GameServer server;
	server.start(); // Uncomment when running the server as a standalone process.

	return 0;
}
