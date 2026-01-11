#pragma once

#include "gameNet/types.hpp"

#include <optional>
#include <string>
#include <variant>

namespace go::gameNet {

// Client Network Events
struct ClientPutStone {
	unsigned x;
	unsigned y;
};
struct ClientPass {};
struct ClientResign {};
struct ClientChat {
	std::string message;
};

// Server Network Events
struct ServerSessionAssign {
	SessionId sessionId;
};
// TODO: Could do a single 'ServerGameUpdate' that contains the delta from the last applied move (including move number for validation).
//       Might be better for a authoritative server.

struct ServerBoardUpdate {
	Seat seat; //!< Only player values.
	unsigned x;
	unsigned y;
};
struct ServerPass {
	Seat seat; //!< Only player values.
};
struct ServerResign {
	Seat seat; //!< Only player values.
};
struct ServerChat {
	Seat seat; //!< Only player values.
	std::string message;
};

using ClientEvent = std::variant<ClientPutStone, ClientPass, ClientResign, ClientChat>;
using ServerEvent = std::variant<ServerSessionAssign, ServerBoardUpdate, ServerChat, ServerPass, ServerResign>;

std::string toMessage(ClientEvent event); //!< Client network event to message string.
std::string toMessage(ServerEvent event); //!< Server network event to message string.

std::optional<ClientEvent> fromClientMessage(const std::string& message); //!< Message string to client network event.
std::optional<ServerEvent> fromServerMessage(const std::string& message); //!< Message string to server network event.

} // namespace go::gameNet
