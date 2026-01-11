#pragma once

#include "gameNet/types.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

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
enum class ServerAction {
	Place,
	Pass,
	Resign,
};
enum class GameStatus {
	Active,
	BlackWin,
	WhiteWin,
	Draw,
};
struct CaptureCoord {
	unsigned x;
	unsigned y;
};
struct ServerDelta {
	unsigned turn;
	Seat seat; //!< Only player values.
	ServerAction action;
	std::optional<unsigned> x;
	std::optional<unsigned> y;
	std::vector<CaptureCoord> captures;
	Seat next; //!< Only player values.
	GameStatus status;
};
struct ServerChat {
	Seat seat; //!< Only player values.
	std::string message;
};

using ClientEvent = std::variant<ClientPutStone, ClientPass, ClientResign, ClientChat>;
using ServerEvent = std::variant<ServerSessionAssign, ServerDelta, ServerChat>;

std::string toMessage(ClientEvent event); //!< Client network event to message string.
std::string toMessage(ServerEvent event); //!< Server network event to message string.

std::optional<ClientEvent> fromClientMessage(const std::string& message); //!< Message string to client network event.
std::optional<ServerEvent> fromServerMessage(const std::string& message); //!< Message string to server network event.

} // namespace go::gameNet
