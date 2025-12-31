#pragma once

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
	std::string sessionKey;
	std::size_t seatIndex; // 0 = Black, 1 = White
};

//! Bridges libNetwork (pure TCP bytes) and the core game (GameEvents).
//! Responsibilities:
//! - Own the TcpServer and translate network lifecycle into a queue of ServerEvents.
//! - Run a dedicated server thread that pops events and forwards them to the game engine.
//! - Maintain per-client session keys to identify players without leaking network concerns into game logic.
//! - Outline chat handling: messages prefixed with CHAT: are forwarded to the opponent.
class GameServer {
public:
	GameServer(Game& game, std::uint16_t port = network::DEFAULT_PORT);
	~GameServer();

	//! Boot the network listener and the server event loop.
	void start();
	//! Signal shutdown to the server loop and stop the network listener.
	void stop();

private:
	// Network callbacks (run on libNetwork threads) just enqueue lightweight events.
	void onClientConnected(std::size_t clientIndex, const asio::ip::tcp::endpoint& endpoint);
	std::optional<std::string> onClientMessage(std::size_t clientIndex, const std::string& payload);
	void onClientDisconnected(std::size_t clientIndex);

	// Server thread: drain queue and act.
	void serverLoop();
	void processEvent(const ServerEvent& event);
	void processClientMessage(const ServerEvent& event);

	//! Translate a validated TryPutStone into the game event queue.
	void handleTryPutStone(std::size_t clientIndex, const Coord& c);
	//! Chat outline: forward to opponent, keep payload raw. Later extend with structured chat packets.
	void forwardChat(std::size_t fromIndex, std::string_view message);

	std::string ensureSession(std::size_t clientIndex);
	std::optional<std::size_t> opponentIndex(std::size_t clientIndex) const;

private:
	Game& m_game;
	network::TcpServer m_network;

	// Session tracking is owned by the server layer (not the network).
	std::array<std::optional<PlayerSession>, network::MAX_PLAYERS> m_sessions;

	// Event queue between network threads and server thread.
	SafeQueue<ServerEvent> m_eventQueue;

	std::thread m_serverThread;
	std::atomic<bool> m_isRunning{false};
};

} // namespace server
} // namespace go
