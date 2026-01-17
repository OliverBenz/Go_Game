#pragma once

#include "gameNet/nwEvents.hpp"
#include "gameNet/types.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace go::gameNet {

//! Callback interface invoked on the client's read thread.
//! \note Keep handlers lightweight.
class IClientHandler {
public:
	virtual ~IClientHandler()                           = default;
	virtual void onGameUpdate(const ServerDelta& event) = 0;
	virtual void onChatMessage(const ServerChat& event) = 0;
	virtual void onDisconnected()                       = 0;
};

class Client {
public:
	Client();
	~Client();

	Client(const Client&)            = delete;
	Client& operator=(const Client&) = delete;
	Client(Client&&)                 = delete;
	Client& operator=(Client&&)      = delete;

	bool registerHandler(IClientHandler* handler); //!< Register a single handler. Returns false if already registered.

	bool connect(const std::string& host);                     //!< Connect to server using default port.
	bool connect(const std::string& host, std::uint16_t port); //!< Connect to server using a custom port.
	void disconnect();                                         //!< Disconnect from the server.
	bool isConnected() const;                                  //!< Check if connected to a server.


	bool send(const ClientEvent& event); //!< Send a client event to the server. Returns false on failure.
	SessionId sessionId() const;         //!< Session id assigned by server. 0 means unassigned.

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide networking protocol stuff.
};

} // namespace go::gameNet
