#pragma once

#include "core/game.hpp"
#include "network/protocol.hpp"
#include "network/server.hpp"

#include "serverEvents.hpp"
#include "sessionManager.hpp"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace go {
namespace server {

//! Game server on two layers.
//! - Network layer    : Communicate with TcpServer, identifies a client through sessionId.
//! - Application layer: Communicate with Game, identifies player through Player enum.
class GameServer {
public:
	GameServer(std::uint16_t port = network::DEFAULT_PORT);
	~GameServer();

	void start(); //!< Boot the network listener and the server event loop.
	void stop();  //!< Signal shutdown to the server loop and stop the network listener.

private:
	// Network callbacks (run on libNetwork threads) just enqueue events.
	void onClientConnected(network::ConnectionId connectionId);
	void onClientMessage(network::ConnectionId connectionId, const network::Message& payload);
	void onClientDisconnected(network::ConnectionId connectionId);

private:
	void serverLoop();                           //!< Server thread: drain queue and act.
	void processEvent(const ServerEvent& event); //!< Server loop calls this. Reads event type and distributes.

	std::optional<Player> freePlayer() const;

private:
	// Processing of server events.
	void processClientMessage(const ServerEvent& event);    //!< Translate payload to network event and handle.
	void processClientConnect(const ServerEvent& event);    //!< Creates session key.
	void processClientDisconnect(const ServerEvent& event); //!< Destroys session key.
	void processShutdown(const ServerEvent& event);         //!< Shutdown server.

private:
	// Processing of the network events that are sent in the server event message payload.
	void handleNetworkEvent(Player player, const network::NwPutStoneEvent& event);
	void handleNetworkEvent(Player player, const network::NwPassEvent& event);
	void handleNetworkEvent(Player player, const network::NwResignEvent& event);
	void handleNetworkEvent(Player player, const network::NwChatEvent& event);

private:
	std::atomic<bool> m_isRunning{false};

	Game m_game{9u};
	std::thread m_gameThread; //!< Runs the game loop.
	bool m_gameReady{false};  //!< Two players connected and assigned a seat.

	network::TcpServer m_network; //!< Communication with clients.

	SessionManager m_sessionManager;
	SafeQueue<ServerEvent> m_eventQueue; //!< Event queue between network threads and server thread.
};

} // namespace server
} // namespace go
