#pragma once

#include "gameNet/nwEvents.hpp"
#include "gameNet/sessionManager.hpp"
#include "gameNet/types.hpp"
#include "network/client.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace go::gameNet {

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

	void connect(const std::string& host, std::uint16_t port = network::DEFAULT_PORT);
	void disconnect();

	bool send(const NwEvent& event);
	bool isConnected() const;

	SessionId sessionId() const;

private:
	void startReadLoop();
	void stopReadLoop();
	void readLoop();
	void handleIncoming(const network::Message& message);

private:
	network::TcpClient m_client;
	std::atomic<bool> m_running{false};
	std::thread m_readThread;

	IClientHandler* m_handler{nullptr};
	SessionId m_sessionId{0};
};

} // namespace go::gameNet
