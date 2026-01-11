#pragma once

#include "gameNet/types.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace go::gameNet {

// Client Network Events
struct ClientPutStone {
	Coord c;
};
struct ClientPass {};
struct ClientResign {};
struct ClientChat {
	std::string message;
};

// Server Events
struct ServerSessionAssign {
	SessionId sessionId; //!< Session Id assigned to player.
};

//! Board update event with relevant data so the client to apply the delta.
struct ServerDelta {
	unsigned turn;               //!< Move number of game.
	Seat seat;                   //!< Player who made move.
	ServerAction action;         //!< Type of move made by player.
	std::optional<Coord> coord;  //!< Coord of place. Set for place action.
	std::vector<Coord> captures; //!< List of captured stones.
	Seat next;                   //!< Next player to make a move.
	GameStatus status;           //!< Game status.
};

struct ServerChat {
	Seat seat;           //!< Only player values.
	std::string message; //!< Chat message.
};


using ClientEvent = std::variant<ClientPutStone, ClientPass, ClientResign, ClientChat>;
using ServerEvent = std::variant<ServerSessionAssign, ServerDelta, ServerChat>;

std::string toMessage(ClientEvent event); //!< Client network event to message string.
std::string toMessage(ServerEvent event); //!< Server network event to message string.

std::optional<ClientEvent> fromClientMessage(const std::string& message); //!< Message string to client network event.
std::optional<ServerEvent> fromServerMessage(const std::string& message); //!< Message string to server network event.

} // namespace go::gameNet
