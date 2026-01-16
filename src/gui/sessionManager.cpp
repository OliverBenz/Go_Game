#include "sessionManager.hpp"

#include "Logging.hpp"

namespace go::gui {

SessionManager::SessionManager() {
	m_network.registerHandler(this);
}
SessionManager::~SessionManager() {
	disconnect();
}

void SessionManager::subscribe(IGameSignalListener* listener, uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void SessionManager::unsubscribe(IGameSignalListener* listener) {
	m_eventHub.unsubscribe(listener);
}


void SessionManager::connect(const std::string& hostIp) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
	}
	m_network.connect(hostIp);
}
void SessionManager::disconnect() {
	m_network.disconnect();
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
	}
	m_eventHub.signal(GS_BoardChange);
	m_eventHub.signal(GS_PlayerChange);
	m_eventHub.signal(GS_StateChange);
}


void SessionManager::tryPlace(unsigned x, unsigned y) {
	m_network.send(gameNet::ClientPutStone{.c = {x, y}});
}
void SessionManager::tryResign() {
	m_network.send(gameNet::ClientResign{});
}
void SessionManager::tryPass() {
	m_network.send(gameNet::ClientPass{});
}
void SessionManager::chat(const std::string& message) {
	m_network.send(gameNet::ClientChat{message});
}

bool SessionManager::isReady() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_gameReady;
}
bool SessionManager::isActive() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.gameActive;
}
Board SessionManager::board() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.board;
}
Player SessionManager::currentPlayer() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.currentPlayer;
}


void SessionManager::onGameUpdate(const gameNet::ServerDelta& event) {
	unsigned lastMoveId = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		lastMoveId = m_position.moveId;
	}

	if (event.turn <= lastMoveId) {
		// Should never happen. Got update twice.
		Logger().Log(Logging::LogLevel::Error, "Game delta sent to client twice.");
		return;
	} else if (event.turn > lastMoveId + 1) {
		// We are missing updates; keep going with best-effort so the game stays playable.
		Logger().Log(Logging::LogLevel::Warning, "Game delta missing updates; applying latest update only.");
	}

	// Update game state
	updateGameState(event);

	// Signal listeners
	switch (event.action) {
	case gameNet::ServerAction::Place:
		m_eventHub.signal(GS_BoardChange);
		m_eventHub.signal(GS_PlayerChange);
		break;
	case gameNet::ServerAction::Pass:
		m_eventHub.signal(GS_PlayerChange);
		if (event.status != gameNet::GameStatus::Active) {
			m_eventHub.signal(GS_StateChange);
		}
		break;
	case gameNet::ServerAction::Resign:
		m_eventHub.signal(GS_StateChange);
		break;
	case gameNet::ServerAction::Count:
		break;
		// TODO: Log
	};
}
void SessionManager::onChatMessage(const gameNet::ServerChat& event) {
	// Do not store here. Has to be stored in GUI anyways.
	// This class is for important stuff. Fuck chat.
}
void SessionManager::onDisconnected() {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
	}
	m_eventHub.signal(GS_BoardChange);
	m_eventHub.signal(GS_PlayerChange);
	m_eventHub.signal(GS_StateChange);
}


void SessionManager::updateGameState(const gameNet::ServerDelta& event) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_position.moveId        = event.turn; // TODO: Align this naming stuff (core, server, gui, etc)
	m_position.gameActive    = event.status == gameNet::GameStatus::Active;
	m_position.currentPlayer = event.next == gameNet::Seat::Black ? Player::Black : Player::White; // TODO: Assert seat is player.
	m_gameReady              = true;

	if (event.action == gameNet::ServerAction::Place) {
		if (event.coord) {
			m_position.board.setAt(Coord{event.coord->x, event.coord->y}, event.seat == gameNet::Seat::Black ? Board::Value::Black : Board::Value::White);
			for (const auto c: event.captures) {
				m_position.board.remAt({c.x, c.y});
			}
		} else {
			Logger().Log(Logging::LogLevel::Warning, "Game delta missing place coordinate; skipping board update.");
		}
	}
	// TODO: status...
}

} // namespace go::gui
