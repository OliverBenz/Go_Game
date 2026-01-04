#pragma once

#include "network/protocol.hpp"
#include "network/server.hpp"

#include "core/game.hpp"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace go {
namespace server {

// Events flowing from network threads into the server thread.
// Keep these small PODs so network callbacks remain cheap.
enum class ServerEventType { ClientConnected, ClientDisconnected, ClientMessage, Shutdown };

struct ServerEvent {
	ServerEventType type{};
	std::string sessionId{}; //!< Assigned per client; used for lightweight auth/tracking.
	std::string payload{};   //!< Raw payload from the client (length-prefixed by libNetwork). Protocol examples: "PUT:3,4", "CHAT:hello".
};

//! Map between network layer and application layer.
struct PlayerSession {
	Player player;         //!< Game layer client identification.
	std::string sessionId; //!< Network layer client identification.
};

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
	void onClientConnected(network::SessionId sessionId);
	void onClientMessage(network::SessionId sessionId, const network::Message& payload);
	void onClientDisconnected(network::SessionId sessionId);

	void serverLoop();                           //!< Server thread: drain queue and act.
	void processEvent(const ServerEvent& event); //!< Server loop calls this. Reads event type and distributes.

	bool isClientConnected(network::SessionId sessionId) const; //!< Do we have an active session with given sessionId.
	std::optional<Player> freePlayer() const;

	network::SessionId sessionFromPlayer(Player player) const;    //!< Get sessionId of a player.
	Player playerFromSession(network::SessionId sessionId) const; //!< Get player colour of corresp. sessionId.

private:                                                    // Processing of server events.
	void processClientMessage(const ServerEvent& event);    //!< Translate payload to network event and handle.
	void processClientConnect(const ServerEvent& event);    //!< Creates session key.
	void processClientDisconnect(const ServerEvent& event); //!< Destroys session key.
	void processShutdown(const ServerEvent& event);         //!< Shutdown server.

private: // Processing of the network events that are sent in the server event message payload.
	void handleNetworkEvent(Player player, const network::NwPutStoneEvent& event);
	void handleNetworkEvent(Player player, const network::NwPassEvent& event);
	void handleNetworkEvent(Player player, const network::NwResignEvent& event);
	void handleNetworkEvent(Player player, const network::NwChatEvent& event);

private:
	Game m_game{9u};
	std::thread m_gameThread;

	network::TcpServer m_network;
	bool m_gameReady{false};

	std::vector<PlayerSession> m_sessions;

	// Event queue between network threads and server thread.
	SafeQueue<ServerEvent> m_eventQueue;
	std::atomic<bool> m_isRunning{false};
};

} // namespace server
} // namespace go
