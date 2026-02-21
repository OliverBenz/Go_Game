
#pragma once

#include "network/server.hpp"
#include <gtest/gtest.h>

namespace go::gtest {

class MockServer : public gameNet::IServerHandler {
public:
	MockServer();
	~MockServer();

	void onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) override;
	void onClientDisconnected(gameNet::SessionId sessionId) override;
	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) override;

	// Handlers for onNetworkEvent
private:
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPutStone& event);
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPass&);
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientResign&);
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientChat& event);

private:
	gameNet::Seat nextSeat(gameNet::Seat seat) const;

private:
	gameNet::Server m_network;
	unsigned m_turn{0u};
};

} // namespace go::gtest
