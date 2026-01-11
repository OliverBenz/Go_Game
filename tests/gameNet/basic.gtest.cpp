#include "mockClient.hpp"
#include "mockServer.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

namespace go::gtest {

TEST(GameNet, Connection) {
	auto server  = MockServer();
	auto client1 = MockClient();
	auto client2 = MockClient();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	client1.chat("Helloooooou");
	client2.tryPlace(0u, 0u);
	client1.tryPlace(1u, 2u);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	client1.disconnect();
	client2.disconnect();
}

} // namespace go::gtest
