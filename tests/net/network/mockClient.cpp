#include "mockClient.hpp"

#include <format>
#include <gtest/gtest.h>
#include <iostream>
#include <optional>

namespace tengen::gtest {

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
	m_network.send(network::ClientChat{message});
}

void MockClient::tryPlace(unsigned x, unsigned y) {
	m_network.send(network::ClientPutStone{.c = {x, y}});
}

static std::string toString(const Player player) {
	return player == Player::Black ? "Black" : "White";
}
static std::string toString(const network::Seat seat) {
	assert(isPlayer(seat));
	return seat == network::Seat::Black ? "Black" : "White";
}


void MockClient::onGameUpdate(const network::ServerDelta& event) {
	const auto seat = toString(event.seat);
	switch (event.action) {
	case network::ServerAction::Place:
		if (event.coord.has_value()) {
			std::cout << std::format("[Client] Received board update from '{}' at ({}, {}).\n", seat, event.coord->x, event.coord->y);
		} else {
			std::cout << std::format("[Client] Received board update from '{}'.\n", seat);
		}
		break;
	case network::ServerAction::Pass:
		std::cout << std::format("[Client] Received pass from '{}'.\n", seat);
		break;
	case network::ServerAction::Resign:
		std::cout << std::format("[Client] Received resign from '{}'\n", seat);
		break;
	case network::ServerAction::Count:
		assert(false && "ServerAction::Count is not a valid action");
		break;
	}
}

void MockClient::onGameConfig(const network::ServerGameConfig& event) {
	std::cout << std::format("[Client] Received config: board={}, komi={}, time={}\n", event.boardSize, event.komi, event.timeSeconds);
}

void MockClient::onChatMessage(const network::ServerChat& event) {
	std::cout << std::format("[Client] Received message from '{}':{}\n", toString(event.player), event.message);
}

void MockClient::onDisconnected() {
	std::cout << std::format("[Client] Client {} disconnected.\n", m_network.sessionId());
}

} // namespace tengen::gtest
