#pragma once

#include "gameNet/protocol.hpp"

#include <string>

namespace go::gameNet {

//! Game specific networking functionality.
class NetworkClient {
public:
	bool connect(const std::string& ip); //!< Connect to server.
	Seat requestPlay();                  //!< Request seat in game.

	bool send(NwEvent event);


private:
	network::TcpClient m_network;
};

} // namespace go::gameNet