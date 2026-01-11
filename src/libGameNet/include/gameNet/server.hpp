#pragma once

#include "gameNet/nwEvents.hpp"
#include "gameNet/types.hpp"

#include <cstdint>
#include <memory>

namespace go::gameNet {

// Callback interface invoked on the server's processing thread.
class IServerHandler {
public:
	virtual ~IServerHandler()                                              = default;
	virtual void onClientConnected(SessionId sessionId, Seat seat)         = 0;
	virtual void onClientDisconnected(SessionId sessionId)                 = 0;
	virtual void onNetworkEvent(SessionId sessionId, const NwEvent& event) = 0;
};

class Server {
public:
	Server();
	explicit Server(std::uint16_t port);
	~Server();

	void start();
	void stop();
	bool registerHandler(IServerHandler* handler);

	bool send(SessionId sessionId, const NwEvent& event); //!< Send event to client with given sessionId.
	bool broadcast(const NwEvent& event);                 //!< Send event to all connected clients.

	Seat getSeat(SessionId sessionId) const; //!< Get the seat connection with a sessionId.

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide networking protocol stuff.
};

} // namespace go::gameNet
