#pragma once

#include "app/eventHub.hpp"
#include "data/board.hpp"
#include "data/gameStatus.hpp"
#include "data/player.hpp"
#include "gameNet/nwEvents.hpp"

namespace go::app {

class Position {
public:
	Position(EventHub& hub); //!< Hub allows to signal listeners on interesting updates.

	void reset(std::size_t boardSize);                 //!< Reset the position to some default data.
	void init(const gameNet::ServerGameConfig& event); //!< Initialize the given position.
	void apply(const gameNet::ServerDelta& delta);     //!< Apply a delta to the current position if ok.
	void setStatus(GameStatus status);                 //!< Update the status.

	const Board& getBoard() const;
	GameStatus getStatus() const;
	Player getPlayer() const;

private:
	bool isDeltaApplicable(const gameNet::ServerDelta& delta); //!< Check if the delta is ok to use for the position update.
	void updatePosition(const gameNet::ServerDelta& delta);    //!< Update the position based on a server delta.
	void signalOnAction(gameNet::ServerAction action);         //!< Signal listeners. Call after applyDelta.

private:
	unsigned m_moveId{0};                  //!< Last move id in game.
	GameStatus m_status{GameStatus::Idle}; //!< Current status of the game.
	Player m_player{Player::Black};
	Board m_board{9u};

	EventHub& m_eventHub; //!< Send events when position changes.
};

} // namespace go::app