#include "app/sessionManager.hpp"

#include "app/gameServer.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>

namespace go::app {

namespace {

void trimInPlace(std::string& text) {
	auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
	auto first   = std::find_if_not(text.begin(), text.end(), isSpace);
	if (first == text.end()) {
		text.clear();
		return;
	}
	auto last = std::find_if_not(text.rbegin(), text.rend(), isSpace).base();
	text.assign(first, last);
}

} // namespace

SessionManager::SessionManager() {
	m_network.registerHandler(this);
}
SessionManager::~SessionManager() {
	disconnect();
}

void SessionManager::signalMask(uint64_t mask) {
	for (uint64_t bit = 1; mask != 0; bit <<= 1) {
		if (mask & bit) {
			m_eventHub.signal(static_cast<AppSignal>(bit));
			mask &= ~bit;
		}
	}
}

void SessionManager::subscribe(IAppSignalListener* listener, uint64_t signalMask) {
	m_eventHub.subscribe(listener, signalMask);
}

void SessionManager::unsubscribe(IAppSignalListener* listener) {
	m_eventHub.unsubscribe(listener);
}


void SessionManager::connect(const std::string& hostIp) {
	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask |= m_position.reset(9u);
		mask |= m_position.setStatus(GameStatus::Ready);
		m_chatHistory.clear();
	}
	m_localServer.reset();
	signalMask(mask | AS_NewChat);
	m_network.connect(hostIp);
}

void SessionManager::host(unsigned boardSize) {
	disconnect();

	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask |= m_position.reset(boardSize);
		mask |= m_position.setStatus(GameStatus::Ready);
		m_chatHistory.clear();
	}
	signalMask(mask | AS_NewChat);

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

	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask |= m_position.reset(9u);
		m_chatHistory.clear();
	}
	signalMask(mask | AS_NewChat);
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
void SessionManager::chat(std::string message) {
	message.erase(std::remove_if(message.begin(), message.end(), [](unsigned char c) { return c == '\r' || c == '\n'; }), message.end());
	trimInPlace(message);
	if (message.empty()) {
		return;
	}
	if (message.size() > kMaxChatMessageBytes) {
		message.resize(kMaxChatMessageBytes);
	}
	m_network.send(gameNet::ClientChat{std::move(message)});
}

SessionManager::ChatSnapshot SessionManager::chatSnapshot(std::size_t fromIndex) const {
	ChatSnapshot snapshot;
	std::lock_guard<std::mutex> lock(m_stateMutex);
	snapshot.total = m_chatHistory.size();
	if (fromIndex > snapshot.total) {
		fromIndex = 0;
	}
	if (fromIndex < snapshot.total) {
		snapshot.entries.assign(m_chatHistory.begin() + static_cast<std::ptrdiff_t>(fromIndex), m_chatHistory.end());
	}
	return snapshot;
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

void SessionManager::onGameUpdate(const gameNet::ServerDelta& event) {
	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask = m_position.apply(event);
	}
	signalMask(mask);
}
void SessionManager::onGameConfig(const gameNet::ServerGameConfig& event) {
	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask = m_position.init(event);
	}
	signalMask(mask);
}
void SessionManager::onChatMessage(const gameNet::ServerChat& event) {
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		auto message = event.message;
		if (message.size() > kMaxChatMessageBytes) {
			message.resize(kMaxChatMessageBytes);
		}
		m_chatHistory.push_back(ChatEntry{.seat = event.seat, .message = std::move(message)});
		if (m_chatHistory.size() > kMaxChatHistory) {
			const auto toTrim = m_chatHistory.size() - kMaxChatHistory;
			m_chatHistory.erase(m_chatHistory.begin(), m_chatHistory.begin() + static_cast<std::ptrdiff_t>(toTrim));
		}
	}
	signalMask(AS_NewChat);
}
void SessionManager::onDisconnected() {
	uint64_t mask = 0u;
	{
		std::lock_guard<std::mutex> lock(m_stateMutex);
		mask |= m_position.reset(9u);
		m_chatHistory.clear();
	}
	signalMask(mask | AS_NewChat);
}

} // namespace go::app
