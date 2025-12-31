#pragma once

#include "network/server.hpp"

namespace go {
namespace server {

class GameServer {
public:
	GameServer(Game& game) : m_game{game} {
		m_network.start();
	}

private:
	Game& m_game;
	network::TcpServer m_network;
};

} // namespace server
} // namespace go
