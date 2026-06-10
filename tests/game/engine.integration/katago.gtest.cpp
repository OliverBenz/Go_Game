#include "engine/katagoSession.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string_view>

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

tengen::engine::KataGoLaunchConfig launchConfig() {
	return tengen::engine::KataGoLaunchConfig{
	        .executablePath = KATAGO_EXECUTABLE,
	        .configPath     = KATAGO_CONFIG,
	        .modelPath      = KATAGO_MODEL,
	};
}

tengen::engine::GameConfig gameConfig() {
	return tengen::engine::GameConfig{
	        .boardSize  = 9u,
	        .komi       = 6.5,
	        .rules      = "chinese",
	        .difficulty = tengen::engine::Difficulty::Custom,
	        .limits     = tengen::engine::SearchLimits{
	                .maxVisits      = 8u,
	                .maxTimeSeconds = std::nullopt,
	        },
	};
}

} // namespace

TEST(KataGoSessionIntegration, SearchesOpeningMove) {
	requireKatagoIntegration();

	tengen::engine::KataGoSession session(launchConfig());
	ASSERT_TRUE(session.newGame(gameConfig())) << session.lastError();

	const auto decision = session.requestMove(tengen::Player::Black);
	ASSERT_TRUE(decision.has_value()) << session.lastError();
	ASSERT_EQ(decision->move.player, tengen::Player::Black);
	ASSERT_EQ(decision->move.kind, tengen::engine::MoveKind::Place);
	ASSERT_TRUE(decision->move.coord.has_value());
	EXPECT_LT(decision->move.coord->x, 9u);
	EXPECT_LT(decision->move.coord->y, 9u);
}

TEST(KataGoSessionIntegration, MirrorsMoveAndRespondsForOpponent) {
	requireKatagoIntegration();

	tengen::engine::KataGoSession session(launchConfig());
	ASSERT_TRUE(session.newGame(gameConfig())) << session.lastError();
	ASSERT_TRUE(session.recordMove(tengen::engine::Move{
	        .kind   = tengen::engine::MoveKind::Place,
	        .player = tengen::Player::Black,
	        .coord  = tengen::Coord{0u, 0u},
	})) << session.lastError();

	const auto decision = session.requestMove(tengen::Player::White);
	ASSERT_TRUE(decision.has_value()) << session.lastError();
	ASSERT_EQ(decision->move.player, tengen::Player::White);
	ASSERT_EQ(decision->move.kind, tengen::engine::MoveKind::Place);
	ASSERT_TRUE(decision->move.coord.has_value());
	EXPECT_LT(decision->move.coord->x, 9u);
	EXPECT_LT(decision->move.coord->y, 9u);
}
