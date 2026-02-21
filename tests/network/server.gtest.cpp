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

namespace go::gtest {

class TestClientHandler final : public gameNet::IClientHandler {
public:
	void onGameUpdate(const gameNet::ServerDelta& event) override {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_lastDelta = event;
		}
		m_cv.notify_all();
	}

	void onGameConfig(const gameNet::ServerGameConfig&) override {
	}

	void onChatMessage(const gameNet::ServerChat&) override {
	}

	void onDisconnected() override {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_disconnected = true;
		m_cv.notify_all();
	}

	bool waitForDelta(std::chrono::milliseconds timeout, gameNet::ServerDelta& out) {
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
	std::optional<gameNet::ServerDelta> m_lastDelta;
	bool m_disconnected{false};
};

class TestServerHandler final : public gameNet::IServerHandler {
public:
	explicit TestServerHandler(gameNet::Server& server) : m_server(server) {
	}

	void onClientConnected(gameNet::SessionId, gameNet::Seat) override {
	}

	void onClientDisconnected(gameNet::SessionId) override {
	}

	void onNetworkEvent(gameNet::SessionId sessionId, const gameNet::ClientEvent& event) override {
		std::visit([&](const auto& e) { handleEvent(sessionId, e); }, event);
	}

private:
	void handleEvent(gameNet::SessionId sessionId, const gameNet::ClientPutStone& event) {
		const auto seat = m_server.getSeat(sessionId);
		if (!gameNet::isPlayer(seat)) {
			return;
		}

		const auto moveId = ++m_turn;
		const auto next   = seat == gameNet::Seat::Black ? gameNet::Seat::White : gameNet::Seat::Black;

		gameNet::ServerDelta delta{
		        .turn     = moveId,
		        .seat     = seat,
		        .action   = gameNet::ServerAction::Place,
		        .coord    = event.c,
		        .captures = {},
		        .next     = next,
		        .status   = gameNet::GameStatus::Active,
		};

		m_server.broadcast(delta);
	}

	template <typename T>
	void handleEvent(gameNet::SessionId, const T&) {
	}

	gameNet::Server& m_server;
	std::atomic<unsigned> m_turn{0};
};

TEST(Networking, ServerDeltaFromPutStone) {
	constexpr std::uint16_t kPort = 12346;

	gameNet::Server server{kPort};
	TestServerHandler serverHandler(server);
	ASSERT_TRUE(server.registerHandler(&serverHandler));
	server.start();

	gameNet::Client client1;
	gameNet::Client client2;
	TestClientHandler handler1;
	TestClientHandler handler2;

	ASSERT_TRUE(client1.registerHandler(&handler1));
	ASSERT_TRUE(client2.registerHandler(&handler2));

	client1.connect("127.0.0.1", kPort);
	client2.connect("127.0.0.1", kPort);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Player 1 places a stone. Valid move
	ASSERT_TRUE(client1.send(gameNet::ClientPutStone{1u, 2u}));

	// Player 2 recives the delta
	gameNet::ServerDelta delta{};
	ASSERT_TRUE(handler2.waitForDelta(std::chrono::milliseconds(300), delta));

	EXPECT_EQ(delta.turn, 1u);
	EXPECT_EQ(delta.seat, gameNet::Seat::Black);
	EXPECT_EQ(delta.action, gameNet::ServerAction::Place);
	ASSERT_TRUE(delta.coord.has_value());
	EXPECT_EQ(delta.coord->x, 1u);
	EXPECT_EQ(delta.coord->y, 2u);
	EXPECT_EQ(delta.captures.size(), 0u);
	EXPECT_EQ(delta.next, gameNet::Seat::White);
	EXPECT_EQ(delta.status, gameNet::GameStatus::Active);

	client1.disconnect();
	client2.disconnect();
	server.stop();
}

} // namespace go::gtest
