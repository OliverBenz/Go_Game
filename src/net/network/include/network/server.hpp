#pragma once

#include "network/nwEvents.hpp"
#include "network/types.hpp"

#include <cstdint>
#include <memory>

namespace go::gameNet {

//! Callback interface invoked on the server's processing thread.
//! \note Keep handlers lightweight.
class IServerHandler {
public:
	virtual ~IServerHandler()                                                  = default;
	virtual void onClientConnected(SessionId sessionId, Seat seat)             = 0;
	virtual void onClientDisconnected(SessionId sessionId)                     = 0;
	virtual void onNetworkEvent(SessionId sessionId, const ClientEvent& event) = 0;
};

class Server {
public:
	Server();
	explicit Server(std::uint16_t port);
	~Server();

	Server(const Server&)            = delete;
	Server& operator=(const Server&) = delete;
	Server(Server&&)                 = delete;
	Server& operator=(Server&&)      = delete;

	void start();
	void stop();

	bool registerHandler(IServerHandler* handler); //!< Register a single handler. Returns false if already registered.

	bool send(SessionId sessionId, const ServerEvent& event); //!< Send event to client with given sessionId. Returns false on failure.
	bool broadcast(const ServerEvent& event);                 //!< Send event to all connected clients. Returns true if any send succeeded.

	Seat getSeat(SessionId sessionId) const; //!< Seat lookup for a session. Returns Seat::None if unknown.

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide networking protocol stuff.
};

} // namespace go::gameNet
