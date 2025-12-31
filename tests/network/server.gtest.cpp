#include "network/server.hpp"
#include "network/client.hpp"

#include <gtest/gtest.h>

namespace go::gtest {

// TODO: Verify board state after every place
TEST(Networking, Server) {
	auto server = network::TcpServer();
	server.start();

	auto client1 = network::TcpClient();
	auto client2 = network::TcpClient();

	client1.connect("127.0.0.1");
	client2.connect("127.0.0.1");

	const auto response1 = client1.send_and_receive("hello from client 1");
	const auto response2 = client2.send_and_receive("hello from client 2");

	client1.disconnect();
	client2.disconnect();
}

} // namespace go::gtest