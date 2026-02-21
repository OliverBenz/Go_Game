#include "app/position.hpp"
#include "logging.hpp"

#include <cassert>

namespace go::app {

void Position::reset(const std::size_t boardSize) {
	m_moveId = 0u;
	m_status = GameStatus::Idle;
	m_player = Player::Black;
	m_board  = Board{boardSize};
}

bool Position::init(const network::ServerGameConfig& event) {
	if (m_status == GameStatus::Active) {
		return false;
	}

	// TODO: Komi and timer not yet implemented.
	m_moveId = 0u;
	m_status = GameStatus::Active;
	m_player = Player::Black;
	m_board  = Board{event.boardSize};
	return true;
}

bool Position::apply(const network::ServerDelta& delta) {
	if (!isDeltaApplicable(delta)) {
		return false;
	}

	m_moveId = delta.turn;
	m_status = delta.status == network::GameStatus::Active ? GameStatus::Active : GameStatus::Done;
	m_player = delta.next == network::Seat::Black ? Player::Black : Player::White;

	if (delta.action == network::ServerAction::Place) {
		if (delta.coord) {
			m_board.place(Coord{delta.coord->x, delta.coord->y}, delta.seat == network::Seat::Black ? Board::Stone::Black : Board::Stone::White);
			for (const auto c: delta.captures) {
				m_board.remove({c.x, c.y});
			}
		} else {
			Logger().Log(Logging::LogLevel::Warning, "Game delta missing place coordinate; skipping board update.");
		}
	}
	return true;
}

void Position::setStatus(GameStatus status) {
	m_status = status;
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

bool Position::isDeltaApplicable(const network::ServerDelta& delta) {
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
	if (!network::isPlayer(delta.seat) || !network::isPlayer(delta.next)) {
		Logger().Log(Logging::LogLevel::Error, "Received game update from non player seat.");
		return false;
	}

	return true;
}

} // namespace go::app
