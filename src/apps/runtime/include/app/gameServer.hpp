#pragma once

#include "core/IGameStateListener.hpp"
#include "core/game.hpp"
#include "data/player.hpp"
#include "network/server.hpp"

#include <string>
#include <thread>
#include <unordered_map>

namespace go {
namespace app {


class GameServer : public gameNet::IServerHandler, public IGameStateListener {
public:
	explicit GameServer(std::size_t boardSize = 9u);
	~GameServer();

	void start(); //!< Boot the network listener and the server event loop.
	void stop();  //!< Signal shutdown to the server loop and stop the network listener.

	// IServerHandler overrides
	void onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) override;
	void onClientDisconnected(gameNet::SessionId sessionId) override;
	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) override;

	// IGameStateListener overrides
	void onGameDelta(const GameDelta& delta) override;

private:
	// Processing of the network events that are sent in the server event message payload.
	void handleNetworkEvent(Player player, const gameNet::ClientPutStone& event);
	void handleNetworkEvent(Player player, const gameNet::ClientPass& event);
	void handleNetworkEvent(Player player, const gameNet::ClientResign& event);
	void handleNetworkEvent(Player player, const gameNet::ClientChat& event);

	struct ChatEntry {
		Player player;
		std::string message;
	};

private:
	Game m_game;
	std::thread m_gameThread; //!< Runs the game loop.

	std::unordered_map<Player, gameNet::SessionId> m_players;
	std::vector<ChatEntry> m_chatHistory;

	gameNet::Server m_server{};
};

} // namespace app
} // namespace go
