#pragma once

#include "app/IAppSignalListener.hpp"
#include "app/eventHub.hpp"
#include "app/position.hpp"
#include "gameNet/client.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace go::app {
class GameServer;

//! Gets game stat delta and constructs a local representation of the game.
//! Listeners can subscribe to certain signals, get notification when happens.
//! Listeners then check which signal and query the updated data from this SessionManager.
//! SessionManager is the local source of truth about the game state, GUI is just dumb renderer of this state.
class SessionManager : public gameNet::IClientHandler {
public:
	struct ChatEntry {
		gameNet::Seat seat;
		std::string message;
	};

	SessionManager();
	~SessionManager();

	void subscribe(IAppSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IAppSignalListener* listener);

	void connect(const std::string& hostIp);
	void host(unsigned boardSize);
	void disconnect();

	// Setters
	void tryPlace(unsigned x, unsigned y);
	void tryResign();
	void tryPass();
	void chat(std::string message);
	struct ChatSnapshot {
		std::size_t total{0};
		std::vector<ChatEntry> entries;
	};
	ChatSnapshot chatSnapshot(std::size_t fromIndex) const;

	// TODO: Maybe the UI elements should have a const reference to 'Position'. (Position is data layer; SessionManager is application layer)
	//       Then position only has public getters and SessionManager is a friend so it can update.
	//       Then we could remove these getters.
	//       SessionManager updates Position and emits signals. Listeners query position for new data.
	// Getters
	GameStatus status() const;
	Board board() const;
	Player currentPlayer() const;

public: // Client listener handlers
	void onGameUpdate(const gameNet::ServerDelta& event) override;
	void onGameConfig(const gameNet::ServerGameConfig& event) override;
	void onChatMessage(const gameNet::ServerChat& event) override;
	void onDisconnected() override;

private:
	void signalMask(uint64_t mask);

	static constexpr std::size_t kMaxChatMessageBytes = 256u;
	static constexpr std::size_t kMaxChatHistory      = 200u;

	gameNet::Client m_network;
	EventHub m_eventHub;
	Position m_position;

	std::vector<ChatEntry> m_chatHistory;
	std::unique_ptr<GameServer> m_localServer;
	mutable std::mutex m_stateMutex;
};

} // namespace go::app
