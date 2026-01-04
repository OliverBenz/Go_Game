#pragma once

#include "network/protocol.hpp"
#include "network/server.hpp"

#include "core/SafeQueue.hpp"
#include "core/gameEvent.hpp"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace go {
class Game;
namespace server {

// Events flowing from network threads into the server thread.
// Keep these small PODs so network callbacks remain cheap.
enum class ServerEventType { ClientConnected, ClientDisconnected, ClientMessage, Shutdown };

struct ServerEvent {
	ServerEventType type{};
	std::size_t clientIndex{}; //!< 0 = Black, 1 = White
	std::string payload{};     //!< Raw payload from the client (length-prefixed by libNetwork). Protocol examples: "PUT:3,4", "CHAT:hello".
	std::string sessionKey{};  //!< Assigned per client; used for lightweight auth/tracking.
};

struct PlayerSession {
	Player player;
	std::string sessionKey;
};

//! Application layer
class GameServer {
public:
	GameServer(Game& game, std::uint16_t port = network::DEFAULT_PORT);
	~GameServer();

	void start(); //!< Boot the network listener and the server event loop.
	void stop();  //!< Signal shutdown to the server loop and stop the network listener.

private:
	// Network callbacks (run on libNetwork threads) just enqueue events.
	void onClientConnected(std::size_t clientIndex, const asio::ip::tcp::endpoint& endpoint);
	void onClientMessage(std::size_t clientIndex, const std::string& payload);
	void onClientDisconnected(std::size_t clientIndex);

	void serverLoop();                           //!< Server thread: drain queue and act.
	void processEvent(const ServerEvent& event); //!< Server loop calls this. Reads event type and distributes.

	std::string ensureSession(std::size_t clientIndex);
	std::optional<std::size_t> opponentIndex(std::size_t clientIndex) const; //!< Client index of opponent.
	Player seatToPlayer(std::size_t clientIndex) const;                      //!< Seat 0 -> Black, Seat 1 -> White.

private:                                                    // Processing of server events.
	void processClientMessage(const ServerEvent& event);    //!< Translate payload to network event and handle.
	void processClientConnect(const ServerEvent& event);    //!< Creates session key.
	void processClientDisconnect(const ServerEvent& event); //!< Destroys session key.
	void processShutdown(const ServerEvent& event);         //!< Shutdown server.

private: // Processing of the network events that are sent in the server event message payload.
	void handleNetworkEvent(const PlayerSession& session, const network::NwPutStoneEvent& event);
	void handleNetworkEvent(const PlayerSession& session, const network::NwPassEvent& event);
	void handleNetworkEvent(const PlayerSession& session, const network::NwResignEvent& event);
	void handleNetworkEvent(const PlayerSession& session, const network::NwChatEvent& event);

private:
	Game& m_game;
	network::TcpServer m_network;

	std::array<std::optional<PlayerSession>, network::MAX_PLAYERS> m_sessions;

	// Event queue between network threads and server thread.
	SafeQueue<ServerEvent> m_eventQueue;

	std::thread m_serverThread;
	std::atomic<bool> m_isRunning{false};
};

} // namespace server
} // namespace go
