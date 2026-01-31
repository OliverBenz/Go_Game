#include "app/position.hpp"
#include "Logging.hpp"

#include <cassert>

namespace go::app {

uint64_t Position::reset(const std::size_t boardSize) {
	m_moveId = 0u;
	m_status = GameStatus::Idle;
	m_player = Player::Black;
	m_board  = Board{boardSize};

	return AS_BoardChange | AS_PlayerChange | AS_StateChange;
}

uint64_t Position::init(const gameNet::ServerGameConfig& event) {
	if (m_status == GameStatus::Active) {
		return AS_None;
	}

	// TODO: Komi and timer not yet implemented.
	m_moveId = 0u;
	m_status = GameStatus::Active;
	m_player = Player::Black;
	m_board  = Board{event.boardSize};

	return AS_BoardChange | AS_PlayerChange | AS_StateChange;
}

uint64_t Position::apply(const gameNet::ServerDelta& delta) {
	if (isDeltaApplicable(delta)) {
		updatePosition(delta);
		return signalMaskOnAction(delta.action);
	}
	return AS_None;
}

uint64_t Position::setStatus(GameStatus status) {
	if (m_status == status) {
		return AS_None;
	}
	m_status = status;
	return AS_StateChange;
}


const Board& Position::getBoard() const {
	return m_board;
}
GameStatus Position::getStatus() const {
	return m_status;
}
Player Position::getPlayer() const {
	return m_player;
}

bool Position::isDeltaApplicable(const gameNet::ServerDelta& delta) {
	// No gamestate updates before game is active (received game configuration).
	if (m_status != GameStatus::Active) {
		Logger().Log(Logging::LogLevel::Error, "Received game update before game is active.");
		return false;
	}

	// Game delta for the proper move.
	if (delta.turn <= m_moveId) {
		Logger().Log(Logging::LogLevel::Error, "Game delta sent to client twice.");
		return false;
	} else if (delta.turn > m_moveId + 1) {
		Logger().Log(Logging::LogLevel::Error, "Game delta missing updates; applying latest update only.");

		// TODO: Query missing move and apply update first.
		return false;
	}

	// Player values valid
	if (!gameNet::isPlayer(delta.seat) || !gameNet::isPlayer(delta.next)) {
		Logger().Log(Logging::LogLevel::Error, "Received game update from non player seat.");
		return false;
	}

	return true;
}

void Position::updatePosition(const gameNet::ServerDelta& delta) {
	m_moveId = delta.turn;
	m_status = delta.status == gameNet::GameStatus::Active ? GameStatus::Active : GameStatus::Done;
	m_player = delta.next == gameNet::Seat::Black ? Player::Black : Player::White;

	if (delta.action == gameNet::ServerAction::Place) {
		if (delta.coord) {
			m_board.place(Coord{delta.coord->x, delta.coord->y}, delta.seat == gameNet::Seat::Black ? Board::Stone::Black : Board::Stone::White);
			for (const auto c: delta.captures) {
				m_board.place({c.x, c.y}, Board::Stone::Empty);
			}
		} else {
			Logger().Log(Logging::LogLevel::Warning, "Game delta missing place coordinate; skipping board update.");
		}
	}
}

uint64_t Position::signalMaskOnAction(gameNet::ServerAction action) {
	switch (action) {
	case gameNet::ServerAction::Place:
		return AS_BoardChange | AS_PlayerChange;
	case gameNet::ServerAction::Pass:
		if (m_status != GameStatus::Active) {
			return AS_PlayerChange | AS_StateChange;
		}
		return AS_PlayerChange;
	case gameNet::ServerAction::Resign:
		return AS_StateChange;
	case gameNet::ServerAction::Count:
		assert(false); //!< This should already be prohibited by libGameNet.
		break;
	};
	return AS_None;
}

} // namespace go::app
