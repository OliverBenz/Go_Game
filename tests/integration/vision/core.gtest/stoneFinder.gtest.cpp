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

//! Count occurrences of a given state.
static std::size_t countState(const std::vector<StoneState>& stones, StoneState s) {
	return static_cast<std::size_t>(std::count(stones.begin(), stones.end(), s));
}

static bool isStone(const StoneState state) {
	return state == StoneState::Black || state == StoneState::White;
}

static std::optional<StoneState> detectSingleStoneInImage(const std::filesystem::path& imagePath) {
	const cv::Mat image = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
	if (image.empty()) {
		return std::nullopt;
	}

	WarpResult warped = warpToBoard(image);
	if (!isValidBoard(warped)) {
		warped = {image, cv::Mat::eye(3, 3, CV_64F)};
	}

	const BoardGeometry geometry   = analyseGeometry(warped);
	const RectifiedBoard rectified = transformImage(image, geometry);
	if (!isValidRectifiedBoard(rectified)) {
		return std::nullopt;
	}

	const StoneResult result = analyseBoard(rectified);
	if (!result.success || result.stones.size() != rectified.geometry.intersections.size()) {
		return std::nullopt;
	}

	auto stoneIt = std::find_if(result.stones.begin(), result.stones.end(), isStone);
	if (stoneIt == result.stones.end()) {
		return std::nullopt;
	}

	// Try to find a second stone.
	const auto stoneIt2 = std::find_if(std::next(stoneIt), result.stones.end(), isStone);
	if (stoneIt2 != result.stones.end()) {
		return std::nullopt;
	}

	return *stoneIt;
}

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

// Checks the board images which are just of a single stone and used to do the setup in the perception algorithm.
// The test is in this project because we check if the gauge stone can be detected. More complex checks on the same images are in the perception.gtest
TEST(StoneFinderUnit, SingleStoneSetup) {
	static constexpr std::array<std::pair<std::string_view, StoneState>, 12> CASES = {{
	        {"C2_1.png", StoneState::Black},
	        {"C2_2.png", StoneState::Black},
	        {"C2_3.png", StoneState::Black},
	        {"C2_4.png", StoneState::Black},
	        {"E3_1.png", StoneState::Black},
	        {"E3_2.png", StoneState::Black},
	        {"E3_3.png", StoneState::Black},
	        {"E3_4.png", StoneState::Black},
	        {"C2_1_white.png", StoneState::White},
	        {"C2_2_white.png", StoneState::White},
	        {"C2_3_white.png", StoneState::White},
	        {"C2_4_white.png", StoneState::White},
	}};

	for (const auto& [fileName, expectedState]: CASES) {
		const auto imagePath = std::filesystem::path(PATH_TEST_IMG) / "setup" / std::string(fileName);
		const auto detected  = detectSingleStoneInImage(imagePath);

		ASSERT_TRUE(detected.has_value()) << imagePath.string();
		EXPECT_EQ(*detected, expectedState) << imagePath.string();
	}
}

} // namespace gtest
} // namespace tengen::vision::core
