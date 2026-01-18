#pragma once

#include "gameNet/client.hpp"

namespace go::gtest {

class MockClient : public gameNet::IClientHandler {
public:
	MockClient();
	~MockClient();

	void disconnect();
	void chat(const std::string& message);
	void tryPlace(unsigned x, unsigned y);

public:
	void onGameUpdate(const gameNet::ServerDelta& event) override;
	void onGameConfig(const gameNet::ServerGameConfig& event) override;
	void onChatMessage(const gameNet::ServerChat& event) override;
	void onDisconnected() override;

private:
	std::string toString(gameNet::Seat seat);

private:
	gameNet::Client m_network;
};

} // namespace go::gtest
