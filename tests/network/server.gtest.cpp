#include "network/server.hpp"
#include "network/client.hpp"

#include "gameNet/protocol.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace go::gtest {

// TODO: Verify board state after every place
TEST(Networking, Server) {
	// auto server = network::TcpServer();
	// server.start();

	auto client1 = network::TcpClient();
	auto client2 = network::TcpClient();

	client1.connect("127.0.0.1");
	client2.connect("127.0.0.1");
	
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	EXPECT_TRUE(client1.send(gameNet::toMessage(gameNet::NwPutStoneEvent{1u, 2u})));
	EXPECT_TRUE(client2.send(gameNet::toMessage(gameNet::NwChatEvent{"Heyo this is my chat message"})));

	EXPECT_TRUE(client1.send(gameNet::toMessage(gameNet::NwPutStoneEvent{1u, 2u})));
	EXPECT_TRUE(client1.send(gameNet::toMessage(gameNet::NwPassEvent{})));

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	const auto c1Session = client1.read();
	const auto c2Session = client2.read();
	
	std::cerr << "Client 1 received:\n\t" << c1Session << "\n";
	std::cerr << "Client 2 received:\n\t" << c2Session << "\n";

	client1.disconnect();
	client2.disconnect();

	
}

} // namespace go::gtest