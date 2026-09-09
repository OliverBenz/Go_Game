#include "testDataHelpers.hpp"

#include "core/serializer.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>

namespace tengen::vision::core {
namespace gtest {


// Test the full image processing pipeline with stone detection at the end.
TEST(Process, Game_Simple_Size9) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_9";

	// Game Information
	static constexpr unsigned MOVES = 13; //!< This game image series has 13 moves.
	// static constexpr double SPACING      = 76.; //!< Pixels between grid lines. Manually checked for this series.
	static constexpr unsigned BOARD_SIZE = 9u; //!< Board size of this game.

	for (unsigned i = 0; i <= MOVES; ++i) {
		std::string fileName = std::format("move_{}.png", i);
		TestResult result    = runPipeline(TEST_PATH / fileName);

		EXPECT_EQ(result.rectified.geometry.boardSize, BOARD_SIZE);
		// EXPECT_NEAR(result.rectified.geometry.spacing, SPACING, SPACING * 0.1); // Allow 5% deviation from expected spacing.
		EXPECT_TRUE(result.stoneStep.success);

		const Board expected = loadExpectedBoard(TEST_PATH / fileName);
		expectStonesMatchBoard(result.stoneStep.stones, BOARD_SIZE, expected);
	}
}

// Test the full image processing pipeline with stone detection at the end.
TEST(Process, Game_Simple_Size13) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_13";

	// Game Information
	static constexpr unsigned MOVES = 27; //!< This game image series has 27 moves.
	// static constexpr double SPACING      = 72.; //!< Pixels between grid lines. Manually checked for this series.
	static constexpr unsigned BOARD_SIZE = 13u; //!< Board size of this game.

	for (unsigned i = 0; i <= MOVES; ++i) {
		std::string fileName = std::format("move_{}.png", i);
		TestResult result    = runPipeline(TEST_PATH / fileName);

		EXPECT_EQ(result.rectified.geometry.boardSize, BOARD_SIZE);
		// EXPECT_NEAR(result.rectified.geometry.spacing, SPACING, SPACING * 0.1); // Allow 5% deviation from expected spacing.

		EXPECT_TRUE(result.stoneStep.success);

		const Board expected = loadExpectedBoard(TEST_PATH / fileName);
		expectStonesMatchBoard(result.stoneStep.stones, BOARD_SIZE, expected);
	}
}

// TODO: Add stone finder for angled_hard
TEST(Process, Board_Detect_Easy) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "angled_easy";

	static constexpr unsigned IMG_COUNT  = 6u;
	static constexpr unsigned BOARD_SIZE = 13u;

	// BoardFinder is only ever rough (see src/vision/core/README.md), so its corner check gets a
	// generous tolerance relative to the B_0 canvas size. GridFinder is expected to be precise, so its
	// corner check gets a tight tolerance relative to one grid spacing (fractions of a stone width).
	static constexpr float BOARD_CORNER_TOLERANCE_FRACTION = 0.10f;
	static constexpr float GRID_CORNER_TOLERANCE_FRACTION  = 0.30f;

	// All angle images show the same physical board, so they share one ground truth file.
	Board expected(0u);
	ASSERT_TRUE(readBoard(TEST_PATH / "board.txt", expected));

	for (unsigned i = 1u; i <= IMG_COUNT; ++i) {
		std::string fileName = std::format("angle_{}.jpeg", i);
		TestResult result    = runPipeline(TEST_PATH / fileName);

		EXPECT_EQ(result.rectified.geometry.boardSize, BOARD_SIZE);

		EXPECT_TRUE(result.stoneStep.success);
		expectStonesMatchBoard(result.stoneStep.stones, BOARD_SIZE, expected);

		const GeometryGroundTruth geometryTruth = loadGeometryGroundTruth(TEST_PATH / fileName);
		ASSERT_EQ(geometryTruth.boardSize, BOARD_SIZE);

		// Stage 1 (BoardFinder): ground truth board corners, warped by H0, should land on imageB0's canvas corners.
		std::vector<cv::Point2f> boardCornersWarped;
		cv::perspectiveTransform(geometryTruth.boardCorners, boardCornersWarped, result.warped.H0);
		const std::vector<cv::Point2f> canvasCorners = {
		        {0.f, 0.f},
		        {static_cast<float>(result.warped.imageB0.cols - 1), 0.f},
		        {static_cast<float>(result.warped.imageB0.cols - 1), static_cast<float>(result.warped.imageB0.rows - 1)},
		        {0.f, static_cast<float>(result.warped.imageB0.rows - 1)},
		};
		const float boardCornerTolerance =
		        BOARD_CORNER_TOLERANCE_FRACTION * static_cast<float>(std::min(result.warped.imageB0.cols, result.warped.imageB0.rows));
		expectPointsMatch(canvasCorners, boardCornersWarped, boardCornerTolerance, "BoardFinder corners");

		// Stage 2 (GridFinder): ground truth grid corners, warped by the refined H, should land on the
		// algorithm's own outermost detected intersections (index = x * boardSize + y).
		std::vector<cv::Point2f> gridCornersWarped;
		cv::perspectiveTransform(geometryTruth.gridCorners, gridCornersWarped, result.rectified.geometry.H);
		const unsigned n                                   = result.rectified.geometry.boardSize;
		const auto& intersections                          = result.rectified.geometry.intersections;
		const std::vector<cv::Point2f> intersectionCorners = {
		        intersections[0],
		        intersections[n - 1],
		        intersections[(n - 1) * n],
		        intersections[n * n - 1],
		};
		ASSERT_GT(result.rectified.geometry.spacing, 0.0);
		const float gridCornerTolerance = GRID_CORNER_TOLERANCE_FRACTION * static_cast<float>(result.rectified.geometry.spacing);
		expectPointsMatch(intersectionCorners, gridCornersWarped, gridCornerTolerance, "GridFinder corners");
	}
}

} // namespace gtest
} // namespace tengen::vision::core
