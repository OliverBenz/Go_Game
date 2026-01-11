#include "gameNet/client.hpp"
#include "gameNet/server.hpp"

#include <chrono>
#include <format>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

namespace go::gtest {

class MockServer : public gameNet::IServerHandler {
public:
	MockServer() {
		EXPECT_TRUE(m_network.registerHandler(this));
		m_network.start();
	}
	~MockServer() {
		m_network.stop();
	}
	void onClientConnected(gameNet::SessionId sessionId, gameNet::Seat seat) override {
		std::cout << std::format("[Server] Client {} connected to seat: {}\n", sessionId, static_cast<int>(seat));
	}
	void onClientDisconnected(gameNet::SessionId sessionId) override {
		std::cout << std::format("[Server] {} disconnected.\n", sessionId);
	}
	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) override {
		std::visit([&](const auto& e) { handleNetworkEvent(sessionId, e); }, event);
	}

private:
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPutStone& event) {
		const auto seat = m_network.getSeat(sessionId);
		m_network.broadcast(gameNet::ServerBoardUpdate{.seat = seat, .x = event.x, .y = event.y});
	}
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientPass&) {
		const auto seat = m_network.getSeat(sessionId);
		m_network.broadcast(gameNet::ServerPass{.seat = seat});
	}
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientResign&) {
		const auto seat = m_network.getSeat(sessionId);
		m_network.broadcast(gameNet::ServerResign{.seat = seat});
	}
	void handleNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientChat& event) {
		const auto seat = m_network.getSeat(sessionId);
		m_network.broadcast(gameNet::ServerChat{.seat = seat, .message = event.message});
	}

private:
	gameNet::Server m_network;
};

class MockClient : public gameNet::IClientHandler {
public:
	MockClient() {
		EXPECT_TRUE(m_network.registerHandler(this));
		m_network.connect("127.0.0.1");
	}
	~MockClient() {
		disconnect();
	}
	void disconnect() {
		m_network.disconnect();
	}

	void chat(const std::string& message) {
		m_network.send(gameNet::ClientChat{message});
	}
	void tryPlace(unsigned x, unsigned y) {
		m_network.send(gameNet::ClientPutStone{x, y});
	}

public:
	void onNetworkEvent(const gameNet::ServerEvent& event) override {
		std::visit([&](const auto& e) { handleNetworkEvent(e); }, event);
	}
	void onDisconnected() override {
		std::cout << std::format("[Client] Client {} disconnected.\n", m_network.sessionId());
	}

private:
	std::string toString(gameNet::Seat seat) {
		assert(isPlayer(seat));
		return seat == gameNet::Seat::Black ? "Black" : "White";
	}

	void handleNetworkEvent(const gameNet::ServerSessionAssign& event) {
		std::cout << std::format("[Client] Received sessionId: '{}'.\n", event.sessionId);
	}
	void handleNetworkEvent(const gameNet::ServerBoardUpdate& event) {
		std::cout << std::format("[Client] Received board update from '{}' at ({}, {}).\n", toString(event.seat), event.x, event.y);
	}
	void handleNetworkEvent(const gameNet::ServerPass& event) {
		std::cout << std::format("[Client] Received pass from '{}'.\n", toString(event.seat));
	}
	void handleNetworkEvent(const gameNet::ServerResign& event) {
		std::cout << std::format("[Client] Received resign from '{}'\n", toString(event.seat));
	}
	void handleNetworkEvent(const gameNet::ServerChat& event) {
		std::cout << std::format("[Client] Received message from '{}':{}\n", toString(event.seat), event.message);
	}

private:
	gameNet::Client m_network;
};

TEST(GameNet, Connection) {
	auto server  = MockServer();
	auto client1 = MockClient();
	auto client2 = MockClient();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	client1.chat("Helloooooou");
	client2.tryPlace(0u, 0u);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	client1.disconnect();
	client2.disconnect();
}

} // namespace go::gtest
