#pragma once

#include "IAppSignalListener.hpp"
#include "core/board.hpp"
#include "eventHub.hpp"
#include "gameNet/client.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace go::app {

class GameServer;

struct Position {
	unsigned moveId{0}; //!< Last move id in game.
	bool gameActive{false};
	Player currentPlayer{Player::Black};

	Board board{9u};
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

	// Setters
	void tryPlace(unsigned x, unsigned y);
	void tryResign();
	void tryPass();
	void chat(const std::string& message);

	// Getters
	bool isReady() const;
	bool isActive() const;
	Board board() const;
	Player currentPlayer() const;

public: // Client listener handlers
	void onGameUpdate(const gameNet::ServerDelta& event) override;
	void onGameConfig(const gameNet::ServerGameConfig& event) override;
	void onChatMessage(const gameNet::ServerChat& event) override;
	void onDisconnected() override;

private:
	void updateGameState(const gameNet::ServerDelta& event);

private:
	bool m_gameReady{false}; //!< Ready to start a game.

	gameNet::Client m_network;
	EventHub m_eventHub;
	Position m_position{};

	std::vector<std::string> m_chatHistory;
	std::unique_ptr<GameServer> m_localServer;
	mutable std::mutex m_stateMutex;
};

} // namespace go::app
