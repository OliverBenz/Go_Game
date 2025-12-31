#include "network/server.hpp"
#include "network/client.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// TODO: Verify board state after every place
TEST(Networking, Server) {
	// auto server = network::TcpServer();
	// server.start();

	auto client1 = network::TcpClient();
	auto client2 = network::TcpClient();

	client1.connect("127.0.0.1");
	client2.connect("127.0.0.1");

	const auto response1 = client1.send_and_receive("CHAT:hello from client 1");
	const auto response2 = client2.send_and_receive("CHAT:hello from client 2");

	const auto response3 = client1.read();
	const auto response4 = client2.read();

	client1.disconnect();
	client2.disconnect();

	std::cerr << "Client 1 received:\n\t" << response1 << "\n\t" << response3 <<"\n";
	std::cerr << "Client 2 received:\n\t" << response2 << "\n\t" << response4 <<"\n";
}

} // namespace go::gtest