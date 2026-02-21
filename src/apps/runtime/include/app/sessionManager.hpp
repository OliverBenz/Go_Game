#pragma once

#include "app/IAppSignalListener.hpp"
#include "app/eventHub.hpp"
#include "app/position.hpp"
#include "network/client.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace go::app {
class GameServer;

struct ChatEntry {
	Player player;
	unsigned messageId;
	std::string message;
};

//! Gets game stat delta and constructs a local representation of the game.
//! Listeners can subscribe to certain signals, get notification when happens.
//! Listeners then check which signal and query the updated data from this SessionManager.
//! SessionManager is the local source of truth about the game state, GUI is just dumb renderer of this state.
class SessionManager : public gameNet::IClientHandler {
public:
	SessionManager();
	~SessionManager();

	void subscribe(IAppSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IAppSignalListener* listener);

	void connect(const std::string& hostIp);
	void host(unsigned boardSize);
	void disconnect();
	void shutdown();

	// Setters
	void tryPlace(unsigned x, unsigned y);
	void tryResign();
	void tryPass();
	void chat(const std::string& message);

	// TODO: Maybe the UI elements should have a const reference to 'Position'. (Position is data layer; SessionManager is application layer)
	//       Then position only has public getters and SessionManager is a friend so it can update.
	//       Then we could remove these getters.
	//       SessionManager updates Position. Position emits signals. Listeners query position for new data.
	// Getters
	GameStatus status() const;
	Board board() const;
	Player currentPlayer() const;
	std::vector<ChatEntry> getChatSince(unsigned messageId) const;

public: // Client listener handlers
	void onGameUpdate(const gameNet::ServerDelta& event) override;
	void onGameConfig(const gameNet::ServerGameConfig& event) override;
	void onChatMessage(const gameNet::ServerChat& event) override;
	void onDisconnected() override;

private:
	gameNet::Client m_network;
	EventHub m_eventHub;
	Position m_position{};

	unsigned m_expectedMessageId{1u};                        //!< Next expected chat message id.
	std::vector<ChatEntry> m_chatHistory{};                  //!< Chat history.
	std::unordered_map<unsigned, ChatEntry> m_pendingChat{}; //!< Messages received out of order.

	std::unique_ptr<GameServer> m_localServer;
	mutable std::mutex m_stateMutex;
};

} // namespace go::app
