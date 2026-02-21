
#pragma once

#include "network/server.hpp"
#include <gtest/gtest.h>

namespace go::gtest {

class MockServer : public network::IServerHandler {
public:
	MockServer();
	~MockServer();

	void onClientConnected(network::SessionId sessionId, network::Seat seat) override;
	void onClientDisconnected(network::SessionId sessionId) override;
	void onNetworkEvent(network::SessionId sessionId, const network::ClientEvent& event) override;

	// Handlers for onNetworkEvent
private:
	void handleNetworkEvent(network::SessionId sessionId, const network::ClientPutStone& event);
	void handleNetworkEvent(network::SessionId sessionId, const network::ClientPass&);
	void handleNetworkEvent(network::SessionId sessionId, const network::ClientResign&);
	void handleNetworkEvent(network::SessionId sessionId, const network::ClientChat& event);

private:
	network::Seat nextSeat(network::Seat seat) const;

private:
	network::Server m_network;
	unsigned m_turn{0u};
};

} // namespace go::gtest
