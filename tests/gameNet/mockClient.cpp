#include "mockClient.hpp"

#include <format>
#include <gtest/gtest.h>
#include <iostream>
#include <optional>

namespace go::gtest {

MockClient::MockClient() {
	EXPECT_TRUE(m_network.registerHandler(this));
	m_network.connect("127.0.0.1");
}

MockClient::~MockClient() {
	disconnect();
}

void MockClient::disconnect() {
	m_network.disconnect();
}

void MockClient::chat(const std::string& message) {
	m_network.send(gameNet::ClientChat{message});
}

void MockClient::tryPlace(unsigned x, unsigned y) {
	m_network.send(gameNet::ClientPutStone{x, y});
}

std::string MockClient::toString(gameNet::Seat seat) {
	assert(isPlayer(seat));
	return seat == gameNet::Seat::Black ? "Black" : "White";
}

void MockClient::onGameUpdate(const gameNet::ServerDelta& event) {
	const auto seat = toString(event.seat);
	switch (event.action) {
	case gameNet::ServerAction::Place:
		std::cout << std::format("[Client] Received board update from '{}' at ({}, {}).\n", seat, event.x.value_or(0u), event.y.value_or(0u));
		break;
	case gameNet::ServerAction::Pass:
		std::cout << std::format("[Client] Received pass from '{}'.\n", seat);
		break;
	case gameNet::ServerAction::Resign:
		std::cout << std::format("[Client] Received resign from '{}'\n", seat);
		break;
	case gameNet::ServerAction::Count:
		assert(false && "ServerAction::Count is not a valid action");
		break;
	}
}

void MockClient::onChatMessage(const gameNet::ServerChat& event) {
	std::cout << std::format("[Client] Received message from '{}':{}\n", toString(event.seat), event.message);
}

void MockClient::onDisconnected() {
	std::cout << std::format("[Client] Client {} disconnected.\n", m_network.sessionId());
}

} // namespace go::gtest