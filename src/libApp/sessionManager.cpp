#include "app/sessionManager.hpp"

#include "Logging.hpp"
#include "app/gameServer.hpp"

namespace go::app {

SessionManager::SessionManager() {
	m_network.registerHandler(this);
}
SessionManager::~SessionManager() {
	disconnect();
}

void SessionManager::subscribe(IAppSignalListener* listener, uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void SessionManager::unsubscribe(IAppSignalListener* listener) {
	m_eventHub.unsubscribe(listener);
}


void SessionManager::connect(const std::string& hostIp) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
		m_chatHistory.clear();
	}
	m_localServer.reset();
	m_network.connect(hostIp);
}

void SessionManager::host(unsigned boardSize) {
	disconnect();

	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position.board         = Board{boardSize};
		m_position.moveId        = 0u;
		m_position.gameActive    = false;
		m_position.currentPlayer = Player::Black;
		m_gameReady              = false;
		m_chatHistory.clear();
	}
	m_eventHub.signal(AS_BoardChange);

	m_localServer = std::make_unique<GameServer>(boardSize);
	m_localServer->start();
	m_network.connect("127.0.0.1");
}

void SessionManager::disconnect() {
	m_network.disconnect();
	if (m_localServer) {
		m_localServer->stop();
		m_localServer.reset();
	}
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
		m_chatHistory.clear();
	}
	m_eventHub.signal(AS_BoardChange);
	m_eventHub.signal(AS_PlayerChange);
	m_eventHub.signal(AS_StateChange);
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
		m_eventHub.signal(AS_BoardChange);
		m_eventHub.signal(AS_PlayerChange);
		break;
	case gameNet::ServerAction::Pass:
		m_eventHub.signal(AS_PlayerChange);
		if (event.status != gameNet::GameStatus::Active) {
			m_eventHub.signal(AS_StateChange);
		}
		break;
	case gameNet::ServerAction::Resign:
		m_eventHub.signal(AS_StateChange);
		break;
	case gameNet::ServerAction::Count:
		break;
		// TODO: Log
	};
}

void SessionManager::onGameConfig(const gameNet::ServerGameConfig& event) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		if (m_position.gameActive) {
			return;
		}
		// TODO: Komi and timer not yet implemented.
		m_position.board  = Board{event.boardSize};
		m_position.moveId = 0u;
	}
	m_eventHub.signal(AS_BoardChange);
}
void SessionManager::onChatMessage(const gameNet::ServerChat& event) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_chatHistory.push_back(event.message);
	}
	m_eventHub.signal(AS_NewChat);
}
void SessionManager::onDisconnected() {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position  = Position{};
		m_gameReady = false;
		m_chatHistory.clear();
	}
	m_eventHub.signal(AS_BoardChange);
	m_eventHub.signal(AS_PlayerChange);
	m_eventHub.signal(AS_StateChange);
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
				m_position.board.setAt({c.x, c.y}, Board::Value::Empty);
			}
		} else {
			Logger().Log(Logging::LogLevel::Warning, "Game delta missing place coordinate; skipping board update.");
		}
	}
	// TODO: status...
}

} // namespace go::app
