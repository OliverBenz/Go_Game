#include "sessionManager.hpp"

namespace go::gui {

SessionManager::~SessionManager() {
	disconnect();
}

void SessionManager::subscribe(IGameSignalListener* listener, uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void SessionManager::unsubscribe(IGameSignalListener* listener) {
	m_eventHub.unsubscribe(listener);
}

void SessionManager::disconnect() {
	m_network.disconnect();
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


const Board& SessionManager::board() const {
	return m_position.board;
}
Player SessionManager::currentPlayer() const {
	return m_position.currentPlayer;
}
bool SessionManager::isActive() const {
	return m_position.gameActive;
}


void SessionManager::onGameUpdate(const gameNet::ServerDelta& event) {
	if (event.turn <= m_position.moveId) {
		// Should never happen. Got update twice.
		Logger().Log(Logging::LogLevel::Error, "Game delta sent to client twice.");
		return;
	} else if (event.turn > m_position.moveId + 1) {
		// We are missing an update. Queue the current and request the old one.
		return;
	}

	// Valid
	updateGameState(event);
}
void SessionManager::onChatMessage(const gameNet::ServerChat& event) {
	// Do not store here. Has to be stored in GUI anyways.
	// This class is for important stuff. Fuck chat.
}
void SessionManager::onDisconnected() {
}


void SessionManager::updateGameState(const gameNet::ServerDelta& event) {
	// TODO: Apply delta to position.
	Logger().Log(Logging::LogLevel::Error, "Delta not implemented");
}

} // namespace go::gui