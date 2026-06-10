#include "engine/katagoSession.hpp"
#include "model/board.hpp"
#include "tengen/botSession.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

namespace {

constexpr const char* KATAGO_EXECUTABLE = TEST_KATAGO_EXECUTABLE;
constexpr const char* KATAGO_CONFIG     = TEST_KATAGO_CONFIG;
constexpr const char* KATAGO_MODEL      = TEST_KATAGO_MODEL;

bool shouldRunKatagoIntegration() {
	const char* env = std::getenv("TENGEN_RUN_KATAGO_INTEGRATION");
	return env && std::string_view{env} == "1";
}

void requireKatagoIntegration() {
	if (!shouldRunKatagoIntegration()) {
		GTEST_SKIP() << "Set TENGEN_RUN_KATAGO_INTEGRATION=1 to run KataGo integration tests.";
	}

	if (std::string_view{KATAGO_EXECUTABLE}.empty() || std::string_view{KATAGO_CONFIG}.empty() || std::string_view{KATAGO_MODEL}.empty()) {
		GTEST_SKIP() << "KataGo test artifacts are not configured.";
	}

	if (!std::filesystem::exists(KATAGO_EXECUTABLE) || !std::filesystem::exists(KATAGO_CONFIG) || !std::filesystem::exists(KATAGO_MODEL)) {
		GTEST_SKIP() << "KataGo executable/config/model paths do not exist on this machine.";
	}
}

bool waitUntil(const std::function<bool()>& predicate) {
	using namespace std::chrono_literals;

	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (std::chrono::steady_clock::now() < deadline) {
		if (predicate()) {
			return true;
		}
		std::this_thread::sleep_for(10ms);
	}

	return predicate();
}

unsigned countStones(const tengen::Board& board, const tengen::Board::Stone stone) {
	unsigned count = 0u;
	for (unsigned y = 0; y != board.size(); ++y) {
		for (unsigned x = 0; x != board.size(); ++x) {
			if (board.get({x, y}) == stone) {
				++count;
			}
		}
	}
	return count;
}

tengen::engine::KataGoLaunchConfig launchConfig() {
	return tengen::engine::KataGoLaunchConfig{
	        .executablePath = KATAGO_EXECUTABLE,
	        .configPath     = KATAGO_CONFIG,
	        .modelPath      = KATAGO_MODEL,
	};
}

} // namespace

TEST(BotSessionIntegration, AppliesBotReplyMove) {
	requireKatagoIntegration();

	auto engine = std::make_unique<tengen::engine::KataGoSession>(launchConfig());
	tengen::app::BotSession session(
	        tengen::app::BotSessionConfig{
	                .boardSize   = 9u,
	                .komi        = 6.5,
	                .rules       = "chinese",
	                .humanPlayer = tengen::Player::Black,
	                .difficulty  = tengen::engine::Difficulty::Custom,
	                .limits      = tengen::engine::SearchLimits{
	                             .maxVisits      = 8u,
	                             .maxTimeSeconds = std::nullopt,
	                },
	        },
	        std::move(engine));

	session.tryPlace(0u, 0u);

	EXPECT_TRUE(waitUntil([&session] {
		const auto board = session.board();
		return board.get({0u, 0u}) == tengen::Board::Stone::Black && countStones(board, tengen::Board::Stone::White) == 1u &&
		       session.currentPlayer() == tengen::Player::Black;
	}));
}
