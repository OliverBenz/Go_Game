#pragma once

#include "app/IAppSignalListener.hpp"
#include "data/board.hpp"
#include "data/gameStatus.hpp"
#include "data/player.hpp"
#include "gameNet/nwEvents.hpp"

namespace go::app {

class Position {
public:
	Position() = default;

	uint64_t reset(std::size_t boardSize);                 //!< Reset the position to some default data. Returns signal mask.
	uint64_t init(const gameNet::ServerGameConfig& event); //!< Initialize the given position. Returns signal mask.
	uint64_t apply(const gameNet::ServerDelta& delta);     //!< Apply a delta to the current position if ok. Returns signal mask.
	uint64_t setStatus(GameStatus status);                 //!< Update the status. Returns signal mask.

	const Board& getBoard() const;
	GameStatus getStatus() const;
	Player getPlayer() const;

private:
	bool isDeltaApplicable(const gameNet::ServerDelta& delta); //!< Check if the delta is ok to use for the position update.
	void updatePosition(const gameNet::ServerDelta& delta);    //!< Update the position based on a server delta.
	uint64_t signalMaskOnAction(gameNet::ServerAction action); //!< Signal mask to emit after applyDelta.

private:
	unsigned m_moveId{0};                  //!< Last move id in game.
	GameStatus m_status{GameStatus::Idle}; //!< Current status of the game.
	Player m_player{Player::Black};
	Board m_board{9u};
};

} // namespace go::app
