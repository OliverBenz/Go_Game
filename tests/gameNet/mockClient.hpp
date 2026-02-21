#pragma once

#include "network/client.hpp"

namespace go::gtest {

class MockClient : public network::IClientHandler {
public:
	MockClient();
	~MockClient();

	void disconnect();
	void chat(const std::string& message);
	void tryPlace(unsigned x, unsigned y);

public:
	void onGameUpdate(const network::ServerDelta& event) override;
	void onGameConfig(const network::ServerGameConfig& event) override;
	void onChatMessage(const network::ServerChat& event) override;
	void onDisconnected() override;

private:
	network::Client m_network;
};

} // namespace go::gtest
