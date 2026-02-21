#include "network/server.hpp"
#include "network/client.hpp"
#include "network/nwEvents.hpp"
#include "network/types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace tengen::gtest {

class TestClientHandler final : public network::IClientHandler {
public:
	void onGameUpdate(const network::ServerDelta& event) override {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_lastDelta = event;
		}
		m_cv.notify_all();
	}

	void onGameConfig(const network::ServerGameConfig&) override {
	}

	void onChatMessage(const network::ServerChat&) override {
	}

	void onDisconnected() override {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_disconnected = true;
		m_cv.notify_all();
	}

	bool waitForDelta(std::chrono::milliseconds timeout, network::ServerDelta& out) {
		std::unique_lock<std::mutex> lock(m_mutex);
		if (!m_cv.wait_for(lock, timeout, [&] { return m_lastDelta.has_value() || m_disconnected; })) {
			return false;
		}
		if (!m_lastDelta.has_value()) {
			return false;
		}
		out = *m_lastDelta;
		return true;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::optional<network::ServerDelta> m_lastDelta;
	bool m_disconnected{false};
};

class TestServerHandler final : public network::IServerHandler {
public:
	explicit TestServerHandler(network::Server& server) : m_server(server) {
	}

	void onClientConnected(network::SessionId, network::Seat) override {
	}

	void onClientDisconnected(network::SessionId) override {
	}

	void onNetworkEvent(network::SessionId sessionId, const network::ClientEvent& event) override {
		std::visit([&](const auto& e) { handleEvent(sessionId, e); }, event);
	}

private:
	void handleEvent(network::SessionId sessionId, const network::ClientPutStone& event) {
		const auto seat = m_server.getSeat(sessionId);
		if (!network::isPlayer(seat)) {
			return;
		}

		const auto moveId = ++m_turn;
		const auto next   = seat == network::Seat::Black ? network::Seat::White : network::Seat::Black;

		network::ServerDelta delta{
		        .turn     = moveId,
		        .seat     = seat,
		        .action   = network::ServerAction::Place,
		        .coord    = event.c,
		        .captures = {},
		        .next     = next,
		        .status   = network::GameStatus::Active,
		};

		m_server.broadcast(delta);
	}

	template <typename T>
	void handleEvent(network::SessionId, const T&) {
	}

	network::Server& m_server;
	std::atomic<unsigned> m_turn{0};
};

TEST(Networking, ServerDeltaFromPutStone) {
	constexpr std::uint16_t kPort = 12346;

	network::Server server{kPort};
	TestServerHandler serverHandler(server);
	ASSERT_TRUE(server.registerHandler(&serverHandler));
	server.start();

	network::Client client1;
	network::Client client2;
	TestClientHandler handler1;
	TestClientHandler handler2;

	ASSERT_TRUE(client1.registerHandler(&handler1));
	ASSERT_TRUE(client2.registerHandler(&handler2));

	client1.connect("127.0.0.1", kPort);
	client2.connect("127.0.0.1", kPort);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Player 1 places a stone. Valid move
	ASSERT_TRUE(client1.send(network::ClientPutStone{1u, 2u}));

	// Player 2 recives the delta
	network::ServerDelta delta{};
	ASSERT_TRUE(handler2.waitForDelta(std::chrono::milliseconds(300), delta));

	EXPECT_EQ(delta.turn, 1u);
	EXPECT_EQ(delta.seat, network::Seat::Black);
	EXPECT_EQ(delta.action, network::ServerAction::Place);
	ASSERT_TRUE(delta.coord.has_value());
	EXPECT_EQ(delta.coord->x, 1u);
	EXPECT_EQ(delta.coord->y, 2u);
	EXPECT_EQ(delta.captures.size(), 0u);
	EXPECT_EQ(delta.next, network::Seat::White);
	EXPECT_EQ(delta.status, network::GameStatus::Active);

	client1.disconnect();
	client2.disconnect();
	server.stop();
}

} // namespace tengen::gtest
