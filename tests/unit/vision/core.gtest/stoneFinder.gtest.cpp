#include "syntheticBoard.hpp"

#include "vision/core/boardFinder.hpp"
#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tengen::vision::core {
namespace gtest {

TEST(StoneFinderUnit, EmptyBoard_NoStones) {
	RectifiedBoard g = makeSyntheticBoard(9u, 80.0, cv::Scalar(80, 140, 200));

	const StoneResult r = analyseBoard(g);
	ASSERT_TRUE(r.success);
	ASSERT_EQ(r.stones.size(), g.geometry.intersections.size());
	ASSERT_EQ(r.confidence.size(), r.stones.size());

	EXPECT_EQ(countState(r.stones, StoneState::Black), 0u);
	EXPECT_EQ(countState(r.stones, StoneState::White), 0u);
}

TEST(StoneFinderUnit, SingleBlackStone_Detected) {
	RectifiedBoard g = makeSyntheticBoard(9u, 80.0, cv::Scalar(80, 140, 200));
	drawStone(g, 4u, 4u, StoneState::Black);

	const StoneResult r = analyseBoard(g);
	ASSERT_TRUE(r.success);

	EXPECT_EQ(countState(r.stones, StoneState::Black), 1u);
	EXPECT_EQ(countState(r.stones, StoneState::White), 0u);
	EXPECT_EQ(r.stones[4u * 9u + 4u], StoneState::Black);
}

TEST(StoneFinderUnit, SingleWhiteStone_Detected) {
	RectifiedBoard g = makeSyntheticBoard(9u, 80.0, cv::Scalar(80, 140, 200));
	drawStone(g, 4u, 4u, StoneState::White);

	const StoneResult r = analyseBoard(g);
	ASSERT_TRUE(r.success);

	EXPECT_EQ(countState(r.stones, StoneState::Black), 0u);
	EXPECT_EQ(countState(r.stones, StoneState::White), 1u);
	EXPECT_EQ(r.stones[4u * 9u + 4u], StoneState::White);
}

TEST(StoneFinderUnit, EdgeWhiteStone_Detected) {
	RectifiedBoard g = makeSyntheticBoard(9u, 80.0, cv::Scalar(80, 140, 200));
	drawStone(g, 0u, 4u, StoneState::White); // on grid edge

	const StoneResult r = analyseBoard(g);
	ASSERT_TRUE(r.success);

	EXPECT_EQ(countState(r.stones, StoneState::Black), 0u);
	EXPECT_EQ(countState(r.stones, StoneState::White), 1u);
	EXPECT_EQ(r.stones[0u * 9u + 4u], StoneState::White);
}

TEST(StoneFinderUnit, BlackStone_WithMildGlare_NotWhite) {
	RectifiedBoard g = makeSyntheticBoard(9u, 80.0, cv::Scalar(80, 140, 200));
	drawStone(g, 4u, 4u, StoneState::Black);

	// Add a small bright highlight inside the black stone (simulates mild glare/reflection).
	const cv::Point2f c = g.geometry.intersections[4u * 9u + 4u];
	cv::circle(g.imageB, c + cv::Point2f(10.0f, -10.0f), 6, cv::Scalar(255, 255, 255), cv::FILLED, cv::LINE_AA);

	const StoneResult r = analyseBoard(g);
	ASSERT_TRUE(r.success);

	EXPECT_EQ(r.stones[4u * 9u + 4u], StoneState::Black);
	EXPECT_EQ(countState(r.stones, StoneState::White), 0u);
}

} // namespace gtest
} // namespace tengen::vision::core
