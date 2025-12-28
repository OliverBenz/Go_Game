#pragma once

#include "network/server.hpp"

namespace go {
namespace server {

class GameServer {
public:
	GameServer(Game& game) : m_game{game} {
	}

private:
	Game& m_game;
	network::TcpServer m_network;
	network::ChatService m_chat;
};

} // namespace server
} // namespace go