#pragma once

#include "core/game.hpp"
#include "gameNet/server.hpp"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace go {
namespace server {

class GameServer : public gameNet::IServerHandler {
public:
	explicit GameServer();
	~GameServer();

	void start(); //!< Boot the network listener and the server event loop.
	void stop();  //!< Signal shutdown to the server loop and stop the network listener.

	void onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) override;
	void onClientDisconnected(gameNet::SessionId sessionId) override;
	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) override;

private:
	// Processing of the network events that are sent in the server event message payload.
	void handleNetworkEvent(Player player, const gameNet::ClientPutStone& event);
	void handleNetworkEvent(Player player, const gameNet::ClientPass& event);
	void handleNetworkEvent(Player player, const gameNet::ClientResign& event);
	void handleNetworkEvent(Player player, const gameNet::ClientChat& event);

private:
	Game m_game{9u};
	std::thread m_gameThread; //!< Runs the game loop.
	bool m_gameReady{false};  //!< Two players connected and assigned a seat.

	std::unordered_map<Player, gameNet::SessionId> m_players;

	gameNet::Server m_server{};
};

} // namespace server
} // namespace go
