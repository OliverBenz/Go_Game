#include "engine/gtp.hpp"

#include <gtest/gtest.h>

using namespace tengen;

TEST(Gtp, ConvertsCoordinatesToVertex) {
	EXPECT_EQ(engine::gtp::toGtpVertex(Coord{0u, 0u}, 9u), "A9");
	EXPECT_EQ(engine::gtp::toGtpVertex(Coord{8u, 8u}, 9u), "J1");
	EXPECT_EQ(engine::gtp::toGtpVertex(Coord{24u, 0u}, 26u), "Z26");
	EXPECT_EQ(engine::gtp::toGtpVertex(Coord{25u, 0u}, 26u), "AA26");
}

TEST(Gtp, ParsesVertexBackToCoordinate) {
	const auto coord = engine::gtp::fromGtpVertex("J1", 9u);
	ASSERT_TRUE(coord.has_value());
	EXPECT_EQ(coord->x, 8u);
	EXPECT_EQ(coord->y, 8u);
}

TEST(Gtp, ParsesSuccessResponseBlock) {
	const auto response = engine::gtp::parseResponseBlock("= D4\n\n");
	ASSERT_TRUE(response.has_value());
	EXPECT_TRUE(response->success);
	EXPECT_EQ(response->payload, "D4");
}

TEST(Gtp, ParsesMoveResponse) {
	const auto response = engine::gtp::parseResponseBlock("= play pass\n\n");
	ASSERT_TRUE(response.has_value());

	const auto move = engine::gtp::parseMoveResponse(*response, Player::White, 19u);
	ASSERT_TRUE(move.has_value());
	EXPECT_EQ(move->kind, engine::MoveKind::Pass);
	EXPECT_EQ(move->player, Player::White);
}
