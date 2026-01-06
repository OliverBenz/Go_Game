#pragma once

#include "network/protocol.hpp"

namespace go::server {

// Events flowing from network threads into the server thread.
// Keep these small PODs so network callbacks remain cheap.
enum class ServerEventType { ClientConnected, ClientDisconnected, ClientMessage, Shutdown };

struct ServerEvent {
	ServerEventType type{};
	network::ConnectionId connectionId{}; //!< Network connection id.
	network::Message payload{};           //!< Network message. Protocol examples: "PUT:3,4", "CHAT:hello".
};

} // namespace go::server