#pragma once

#include "gameNet/nwEvents.hpp"
#include "gameNet/types.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace go::gameNet {

// Callback interface invoked on the client's read thread.
class IClientHandler {
public:
	virtual ~IClientHandler()                           = default;
	virtual void onSessionAssigned(SessionId sessionId) = 0;
	virtual void onNetworkEvent(const NwEvent& event)   = 0;
	virtual void onDisconnected()                       = 0;
};

class Client {
public:
	Client();
	~Client();

	bool registerHandler(IClientHandler* handler);

	void connect(const std::string& host);
	void connect(const std::string& host, std::uint16_t port);
	void disconnect();
	bool isConnected() const;

	bool send(const NwEvent& event);

	SessionId sessionId() const;

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide networking protocol stuff.
};

} // namespace go::gameNet
