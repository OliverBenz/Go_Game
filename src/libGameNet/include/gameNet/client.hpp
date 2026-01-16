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

	bool registerHandler(IClientHandler* handler);

	bool connect(const std::string& host);
	bool connect(const std::string& host, std::uint16_t port);
	void disconnect();
	bool isConnected() const;

	bool send(const ClientEvent& event);

	SessionId sessionId() const;

private:
	class Implementation;
	std::unique_ptr<Implementation> m_pimpl; //!< Pimpl to hide networking protocol stuff.
};

} // namespace go::gameNet
