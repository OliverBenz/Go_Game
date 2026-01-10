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
		m_network.registerHandler(this);
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
	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::NwEvent& event) override {
		std::cout << std::format("[Server] {} sent event.\n", sessionId);
	}

private:
	gameNet::Server m_network;
};

class MockClient : public gameNet::IClientHandler {
public:
	MockClient() {
		m_network.registerHandler(this);
		m_network.connect("127.0.0.1");
	}
	~MockClient() {
		m_network.disconnect();
	}

	void chat(const std::string& message) {
		m_network.send(gameNet::NwChatEvent{message});
	}
	void tryPlace(unsigned x, unsigned y) {
		m_network.send(gameNet::NwPutStoneEvent{x, y});
	}

public:
	void onSessionAssigned(gameNet::SessionId sessionId) override {
		// TODO: Does not give me the seat (or player colour)
		std::cout << std::format("[Client] Got a session: {}.\n", sessionId);
	}
	void onNetworkEvent(const gameNet::NwEvent& event) override {
		std::visit([&](const auto& e) { handleNetworkEvent(e); }, event);
	}
	void onDisconnected() override {
		std::cout << std::format("[Client] Client {} disconnected.\n", m_network.sessionId());
	}

private:
	void handleNetworkEvent(const gameNet::NwPutStoneEvent& event) {
		std::cout << std::format("Client {} received Event PutStone at ({}, {})", m_network.sessionId(), event.x, event.y);
	}
	void handleNetworkEvent(const gameNet::NwPassEvent& event) {
		std::cout << std::format("Client {} received Event Pass.", m_network.sessionId());
	}
	void handleNetworkEvent(const gameNet::NwResignEvent& event) {
		std::cout << std::format("Client {} received Event Resign", m_network.sessionId());
	}
	void handleNetworkEvent(const gameNet::NwChatEvent& event) {
		std::cout << std::format("Client {} received message: {}", m_network.sessionId(), event.message);
	}

private:
	gameNet::Client m_network;
};

TEST(GameNet, Connection) {
	// TODO: Update threading. The server/client should not run in main thread.
	auto server  = MockServer();
	auto client1 = MockClient();
	auto client2 = MockClient();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
	client1.chat("Helloooooo");
	client2.tryPlace(0u, 0u);

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

} // namespace go::gtest