#include "app/sessionManager.hpp"

#include "Logging.hpp"
#include "app/gameServer.hpp"

#include <algorithm>
#include <cassert>

namespace go::app {

SessionManager::SessionManager() : m_position{m_eventHub} {
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
		m_position.reset(9u);
		m_position.setStatus(GameStatus::Ready);
		m_expectedMessageId = 1u;
		m_chatHistory.clear();
	}
	m_localServer.reset();
	m_network.connect(hostIp);
}

void SessionManager::host(unsigned boardSize) {
	disconnect();

	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		m_position.reset(boardSize);
		m_position.setStatus(GameStatus::Ready);
		m_expectedMessageId = 1u;
		m_chatHistory.clear();
	}

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

	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_position.reset(9u);
	m_expectedMessageId = 1u;
	m_chatHistory.clear();
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

GameStatus SessionManager::status() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getStatus();
}
Board SessionManager::board() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getBoard();
}
Player SessionManager::currentPlayer() const {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	return m_position.getPlayer();
}
std::vector<ChatEntry> SessionManager::getChatSince(const unsigned messageId) const {
	std::lock_guard<std::mutex> lock(m_stateMutex);

	// Find first entry with id > messageId
	auto it = std::upper_bound(m_chatHistory.begin(), m_chatHistory.end(), messageId,
	                           [](const unsigned value, const ChatEntry& e) { return e.messageId > value; });

	return {it, m_chatHistory.end()};
}

void SessionManager::onGameUpdate(const gameNet::ServerDelta& event) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_position.apply(event);
}
void SessionManager::onGameConfig(const gameNet::ServerGameConfig& event) {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_position.init(event);
}
void SessionManager::onChatMessage(const gameNet::ServerChat& event) {
	std::lock_guard<std::mutex> lock(m_stateMutex);

	if (event.messageId == m_expectedMessageId) {
		m_chatHistory.emplace_back(ChatEntry{event.player, event.messageId, event.message});
		++m_expectedMessageId;
	} else {
		// TODO: Handle missing messages & ordered inserts (messageId assumed ordered in GUI)
	}
	m_eventHub.signal(AS_NewChat);
}
void SessionManager::onDisconnected() {
	std::lock_guard<std::mutex> lock(m_stateMutex);
	m_position.reset(9u);
	m_expectedMessageId = 1u;
	m_chatHistory.clear();
}

} // namespace go::app
