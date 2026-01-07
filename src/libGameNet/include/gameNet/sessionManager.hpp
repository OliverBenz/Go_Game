#pragma once

#include "gameNet/types.hpp"
#include "network/protocol.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace go::gameNet {

using SessionId = std::uint32_t;

struct SessionContext {
	network::ConnectionId connectionId; //!< Identify connection on network layer.
	SessionId sessionId;                //!< Identify connection on application layer.

	Seat seat;     //!< Role in the game.
	bool isActive; //!< Connected or disconnected.
};

class SessionManager {
public:
	SessionId add(network::ConnectionId connectionId); //!< Register a new session.
	void remove(SessionId sessionId);                  //!< Remove a session context.

	SessionId getSessionId(network::ConnectionId connectionId) const; //!< Get sessionId connected with a connection.
	network::ConnectionId getConnectionId(SessionId sessionId) const; //!< Get connectionId connected with a sessionId.
	network::ConnectionId getConnectionIdBySeat(Seat seat) const;     //!< Get connectionId for an active seat.

	Seat getSeat(SessionId sessionId);
	void setSeat(SessionId sessionId, Seat seat); //!< Set the seat of a session.

	void setDisconnected(SessionId sessionId); //!< Mark the given session as inactive.

	// TODO: This does not relly belong to sessionManager but the data is nicely available
	bool gameReady() const; //!< True if both Black and White seats are assigned and connected.

private:
	SessionId generateSessionId() const; //!< Generate unique sessionId.

private:
	std::unordered_map<SessionId, SessionContext> m_sessions;
	std::unordered_map<network::ConnectionId, SessionId> m_connectionToSession;
};

} // namespace go::gameNet
