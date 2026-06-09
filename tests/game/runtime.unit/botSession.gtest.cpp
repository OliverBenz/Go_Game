#include "model/board.hpp"
#include "tengen/botSession.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace {

class FakeEngineSession final : public tengen::engine::IEngineSession {
public:
	bool newGame(const tengen::engine::GameConfig& config) override {
		m_config = config;
		m_state  = tengen::engine::SessionState::Ready;
		return true;
	}

	bool recordMove(const tengen::engine::Move& move) override {
		m_recordedMoves.push_back(move);
		return true;
	}

	std::optional<tengen::engine::Decision> requestMove(const tengen::Player player) override {
		if (m_requested) {
			return std::nullopt;
		}

		m_requested = true;
		return tengen::engine::Decision{
		        .move = tengen::engine::Move{
		                .kind   = tengen::engine::MoveKind::Place,
		                .player = player,
		                .coord  = tengen::Coord{1u, 0u},
		        },
		};
	}

	tengen::engine::SessionState state() const override {
		return m_state;
	}

	std::string lastError() const override {
		return {};
	}

	void shutdown() override {
		m_state = tengen::engine::SessionState::Closed;
	}

	const std::vector<tengen::engine::Move>& recordedMoves() const {
		return m_recordedMoves;
	}

private:
	tengen::engine::GameConfig m_config{};
	std::vector<tengen::engine::Move> m_recordedMoves{};
	tengen::engine::SessionState m_state{tengen::engine::SessionState::Idle};
	bool m_requested{false};
};

bool waitUntil(const std::function<bool()>& predicate) {
	using namespace std::chrono_literals;

	const auto deadline = std::chrono::steady_clock::now() + 500ms;
	while (std::chrono::steady_clock::now() < deadline) {
		if (predicate()) {
			return true;
		}
		std::this_thread::sleep_for(10ms);
	}

	return predicate();
}

} // namespace

TEST(BotSession, AppliesHumanAndBotMovesThroughCoreGame) {
	auto engine = std::make_unique<FakeEngineSession>();
	auto* raw   = engine.get();

	tengen::app::BotSession session(
	        tengen::app::BotSessionConfig{
	                .boardSize   = 9u,
	                .komi        = 6.5,
	                .rules       = "chinese",
	                .humanPlayer = tengen::Player::Black,
	                .difficulty  = tengen::engine::Difficulty::Easy,
	        },
	        std::move(engine));

	session.tryPlace(0u, 0u);

	EXPECT_TRUE(waitUntil([&session] {
		const auto board = session.board();
		return board.get({0u, 0u}) == tengen::Board::Stone::Black && board.get({1u, 0u}) == tengen::Board::Stone::White;
	}));

	session.shutdown();

	ASSERT_EQ(raw->recordedMoves().size(), 2u);
	EXPECT_EQ(raw->recordedMoves()[0].player, tengen::Player::Black);
	EXPECT_EQ(raw->recordedMoves()[1].player, tengen::Player::White);
	EXPECT_EQ(session.currentPlayer(), tengen::Player::Black);
}
