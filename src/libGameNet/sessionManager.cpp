#include "gameNet/sessionManager.hpp"

#include <cassert>

namespace go::gameNet {

static SessionId nextSessionId{1};

SessionId SessionManager::add(network::ConnectionId connectionId) {
	const auto sessionId = generateSessionId();
	SessionContext context{
	        .connectionId = connectionId,
	        .sessionId    = sessionId,
	        .seat         = Seat::None,
	        .isActive     = true,
	};
	m_sessions.emplace(sessionId, context);
	m_connectionToSession.emplace(connectionId, sessionId);
	return sessionId;
}

void SessionManager::remove(SessionId sessionId) {
	const auto it = m_sessions.find(sessionId);
	if (it == m_sessions.end()) {
		return;
	}
	m_connectionToSession.erase(it->second.connectionId);
	m_sessions.erase(it);
}

SessionId SessionManager::getSessionId(network::ConnectionId connectionId) const {
	const auto it = m_connectionToSession.find(connectionId);
	if (it == m_connectionToSession.end()) {
		return 0;
	}
	return it->second;
}

network::ConnectionId SessionManager::getConnectionId(SessionId sessionId) const {
	const auto it = m_sessions.find(sessionId);
	if (it == m_sessions.end()) {
		return 0;
	}
	return it->second.connectionId;
}

network::ConnectionId SessionManager::getConnectionIdBySeat(Seat seat) const {
	assert(seat == Seat::Black || seat == Seat::White);

	for (const auto& [_, context]: m_sessions) {
		if (context.isActive && context.seat == seat) {
			return context.connectionId;
		}
	}
	return 0;
}

Seat SessionManager::getSeat(SessionId sessionId) const {
	const auto it = m_sessions.find(sessionId);
	if (it == m_sessions.end()) {
		return Seat::None;
	}
	return it->second.seat;
}

void SessionManager::setSeat(SessionId sessionId, Seat seat) {
	const auto it = m_sessions.find(sessionId);
	if (it == m_sessions.end()) {
		return;
	}
	it->second.seat = seat;
}

void SessionManager::setDisconnected(SessionId sessionId) {
	const auto it = m_sessions.find(sessionId);
	if (it == m_sessions.end()) {
		return;
	}
	it->second.isActive = false;
}

void SessionManager::forEachSession(const std::function<void(const SessionContext&)>& visitor) const {
	for (const auto& [_, context]: m_sessions) {
		visitor(context);
	}
}

SessionId SessionManager::generateSessionId() const {
	SessionId candidate = nextSessionId;
	if (candidate == 0) {
		candidate = 1;
	}
	while (m_sessions.find(candidate) != m_sessions.end()) {
		++candidate;
		if (candidate == 0) {
			++candidate;
		}
	}

	nextSessionId = candidate + 1;
	if (nextSessionId == 0) {
		nextSessionId = 1;
	}
	return candidate;
}


} // namespace go::gameNet
