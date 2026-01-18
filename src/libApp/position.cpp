#include "app/position.hpp"
#include "Logging.hpp"

#include <cassert>

namespace go::app {

Position::Position(EventHub& hub) : m_eventHub{hub} {
}

void Position::reset(const std::size_t boardSize) {
	m_moveId = 0u;
	m_status = GameStatus::Idle;
	m_player = Player::Black;
	m_board  = Board{boardSize};

	m_eventHub.signal(AS_BoardChange);
	m_eventHub.signal(AS_PlayerChange);
	m_eventHub.signal(AS_StateChange);
}

void Position::init(const gameNet::ServerGameConfig& event) {
	if (m_status == GameStatus::Active) {
		return;
	}

	// TODO: Komi and timer not yet implemented.
	m_moveId = 0u;
	m_status = GameStatus::Active;
	m_player = Player::Black;
	m_board  = Board{event.boardSize};

	m_eventHub.signal(AS_BoardChange);
	m_eventHub.signal(AS_PlayerChange);
	m_eventHub.signal(AS_StateChange);
}

void Position::apply(const gameNet::ServerDelta& delta) {
	if (isDeltaApplicable(delta)) {
		updatePosition(delta);
		signalOnAction(delta.action);
	}
}

void Position::setStatus(GameStatus status) {
	m_status = status;
	m_eventHub.signal(AS_StateChange);
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
			m_board.setAt(Coord{delta.coord->x, delta.coord->y}, delta.seat == gameNet::Seat::Black ? Board::Value::Black : Board::Value::White);
			for (const auto c: delta.captures) {
				m_board.setAt({c.x, c.y}, Board::Value::Empty);
			}
		} else {
			Logger().Log(Logging::LogLevel::Warning, "Game delta missing place coordinate; skipping board update.");
		}
	}
}

void Position::signalOnAction(gameNet::ServerAction action) {
	switch (action) {
	case gameNet::ServerAction::Place:
		m_eventHub.signal(AS_BoardChange);
		m_eventHub.signal(AS_PlayerChange);
		break;
	case gameNet::ServerAction::Pass:
		m_eventHub.signal(AS_PlayerChange);
		if (m_status != GameStatus::Active) {
			m_eventHub.signal(AS_StateChange);
		}
		break;
	case gameNet::ServerAction::Resign:
		m_eventHub.signal(AS_StateChange);
		break;
	case gameNet::ServerAction::Count:
		assert(false); //!< This should already be prohibited by libGameNet.
		break;
	};
}

} // namespace go::app