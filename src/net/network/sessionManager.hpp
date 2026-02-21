#pragma once

#include "network/core/protocol.hpp"
#include "network/types.hpp"

#include <functional>
#include <unordered_map>

namespace tengen::network {

struct SessionContext {
	core::ConnectionId connectionId; //!< Identify connection on network layer.
	SessionId sessionId;             //!< Identify connection on application layer.

	Seat seat;     //!< Role in the game.
	bool isActive; //!< Connected or disconnected.
};

class SessionManager {
public:
	SessionId add(core::ConnectionId connectionId); //!< Register a new session.
	void remove(SessionId sessionId);               //!< Remove a session context.

	SessionId getSessionId(core::ConnectionId connectionId) const; //!< Get sessionId connected with a connection.
	core::ConnectionId getConnectionId(SessionId sessionId) const; //!< Get connectionId connected with a sessionId.
	core::ConnectionId getConnectionIdBySeat(Seat seat) const;     //!< Get connectionId for an active seat.

	Seat getSeat(SessionId sessionId) const;
	void setSeat(SessionId sessionId, Seat seat); //!< Set the seat of a session.

	void setDisconnected(SessionId sessionId); //!< Mark the given session as inactive.

	void forEachSession(const std::function<void(const SessionContext&)>& visitor) const;

private:
	SessionId generateSessionId() const; //!< Generate unique sessionId.

private:
	// Note: This manager is used from the server processing thread only.
	std::unordered_map<SessionId, SessionContext> m_sessions;
	std::unordered_map<core::ConnectionId, SessionId> m_connectionToSession;
};

} // namespace tengen::network
