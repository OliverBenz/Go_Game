#pragma once

#include "IGameSignalListener.hpp"
#include "Logging.hpp"
#include "core/board.hpp"
#include "eventHub.hpp"
#include "gameNet/client.hpp"

namespace go::gui {

struct Position {
	unsigned moveId{0}; //!< Last move id in game.
	bool gameActive{false};
	Player currentPlayer{Player::Black};

	Board board{13u};
};

//! Gets game stat delta and constructs a local representation of the game.
//! Listeners can subscribe to certain signals, get notification when happens.
//! Listeners then check which signal and query the updated data from this SessionManager.
//! SessionManager is the local source of truth about the game state, GUI is just dumb renderer of this state.
class SessionManager : public gameNet::IClientHandler {
public:
	~SessionManager();

	void subscribe(IGameSignalListener* listener, uint64_t signalMask);
	void unsubscribe(IGameSignalListener* listener);
	void disconnect();

	// Setters
	void tryPlace(unsigned x, unsigned y);
	void tryResign();
	void tryPass();
	void chat(const std::string& message);

	// Getters
	const Board& board() const;
	Player currentPlayer() const;
	bool isActive() const;

public: // Client listener handlers
	void onGameUpdate(const gameNet::ServerDelta& event) override;
	void onChatMessage(const gameNet::ServerChat& event) override;
	void onDisconnected() override;

private:
	void updateGameState(const gameNet::ServerDelta& event);

private:
	gameNet::Client m_network;
	EventHub m_eventHub;
	Position m_position{};
};

} // namespace go::gui