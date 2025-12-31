#pragma once

#include "network/server.hpp"
#include <chrono>
namespace go {
namespace server {

class GameServer {
public:
	GameServer(Game& game) : m_game{game} {
		using namespace std::chrono_literals;

		m_network.start();
		std::this_thread::sleep_for(20s);
	}

private:
	Game& m_game;
	network::TcpServer m_network;
};

} // namespace server
} // namespace go
