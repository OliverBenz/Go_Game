#pragma once

#include "network/core/protocol.hpp"

namespace go::network {

// Events flowing from network threads into the server thread.
// Keep these small PODs so network callbacks remain cheap.
enum class ServerQueueEventType { ClientConnected, ClientDisconnected, ClientMessage, Shutdown };

struct ServerQueueEvent {
	ServerQueueEventType type{};
	core::ConnectionId connectionId{}; //!< Network connection id.
	core::Message payload{};           //!< Network message. Protocol examples: {"type":"put","x":3,"y":4}, {"type":"chat","message":"hello"}.
};

} // namespace go::network
