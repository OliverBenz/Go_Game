#pragma once

#include "gameNet/types.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace go::gameNet {

// Client Network Events (client -> server)
struct ClientPutStone {
	Coord c;
};
struct ClientPass {};
struct ClientResign {};
struct ClientChat {
	std::string message;
};

// Server Events (server -> client)
struct ServerSessionAssign {
	SessionId sessionId; //!< Session Id assigned to player.
};

// Game configuration sent by server to clients.
struct ServerGameConfig {
	unsigned boardSize;
	double komi;
	unsigned timeSeconds;
};

//! Board update event with relevant data so the client can apply the delta.
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
using ServerEvent = std::variant<ServerSessionAssign, ServerGameConfig, ServerDelta, ServerChat>;

// Serialize typed events to JSON messages.
std::string toMessage(ClientEvent event);
std::string toMessage(ServerEvent event);

// Parse JSON messages into typed events. Returns empty on invalid input.
std::optional<ClientEvent> fromClientMessage(const std::string& message);
std::optional<ServerEvent> fromServerMessage(const std::string& message);

} // namespace go::gameNet
