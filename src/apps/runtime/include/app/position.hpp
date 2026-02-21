#pragma once

#include "data/board.hpp"
#include "data/gameStatus.hpp"
#include "data/player.hpp"
#include "network/nwEvents.hpp"

namespace go::app {

class Position {
public:
	Position() = default;

	void reset(std::size_t boardSize);                 //!< Reset the position to some default data.
	bool init(const network::ServerGameConfig& event); //!< Initialize the given position. Returns true if it changed state.
	bool apply(const network::ServerDelta& delta);     //!< Apply a delta to the current position if ok.
	void setStatus(GameStatus status);                 //!< Update the status.

	const Board& getBoard() const;
	GameStatus getStatus() const;
	Player getPlayer() const;

private:
	bool isDeltaApplicable(const network::ServerDelta& delta); //!< Check if the delta is ok to use for the position update.

private:
	unsigned m_moveId{0};                  //!< Last move id in game.
	GameStatus m_status{GameStatus::Idle}; //!< Current status of the game.
	Player m_player{Player::Black};
	Board m_board{9u};
};

} // namespace go::app
