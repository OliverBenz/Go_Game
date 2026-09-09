#include "core/serializer.hpp"
#include "vision/core/boardFinder.hpp"
#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <filesystem>

namespace tengen::vision::core {
namespace gtest {

//! Expected test results (result of each step in the pipeline).
struct TestResult {
	WarpResult warped;
	RectifiedBoard rectified;
	StoneResult stoneStep;
};

//! Run the stone detection pipeline. Ensure intermediate steps are generally valid. Return test result.
TestResult runPipeline(const std::filesystem::path& imgPath) {
	std::cout << "Running test: " << imgPath.string() << '\n';

	cv::Mat image = cv::imread(imgPath.string());
	EXPECT_FALSE(image.empty());

	// Warp image roughly around the board.
	WarpResult warped = warpToBoard(image);
	EXPECT_FALSE(warped.imageB0.empty());
	EXPECT_FALSE(warped.H0.empty());

	// Properly construct the board geometry.
	const BoardGeometry geometry = analyseGeometry(warped);
	EXPECT_TRUE(isValidGeometry(geometry));
	RectifiedBoard rectified = transformImage(image, geometry);
	EXPECT_TRUE(isValidRectifiedBoard(rectified));
	EXPECT_FALSE(rectified.geometry.H.empty());
	EXPECT_FALSE(rectified.geometry.intersections.empty());
	EXPECT_TRUE(rectified.geometry.intersections.size() == rectified.geometry.boardSize * rectified.geometry.boardSize);
	EXPECT_TRUE(rectified.geometry.boardSize == 9 || rectified.geometry.boardSize == 13 || rectified.geometry.boardSize == 19);

	// Find the stones on the board.
	StoneResult stoneRes = analyseBoard(rectified);
	EXPECT_TRUE(stoneRes.success);
	EXPECT_EQ(stoneRes.stones.size(), rectified.geometry.intersections.size());

	return {warped, rectified, stoneRes};
}

//! Count how many black stones are present in a StoneState list.
std::size_t blackStoneCount(const std::vector<StoneState>& stones) {
	return static_cast<std::size_t>(std::count(stones.begin(), stones.end(), StoneState::Black));
}

//! Count how many white stones are present in a StoneState list.
std::size_t whiteStoneCount(const std::vector<StoneState>& stones) {
	return static_cast<std::size_t>(std::count(stones.begin(), stones.end(), StoneState::White));
}

//! Count how many stones are present in a StoneState list (black + white).
std::size_t stoneCount(const std::vector<StoneState>& stones) {
	return blackStoneCount(stones) + whiteStoneCount(stones);
}

//! Map a ground truth board stone to the vision StoneState it corresponds to.
StoneState toStoneState(const Board::Stone stone) {
	switch (stone) {
	case Board::Stone::Black:
		return StoneState::Black;
	case Board::Stone::White:
		return StoneState::White;
	case Board::Stone::Empty:
		return StoneState::Empty;
	}
	return StoneState::Empty;
}

//! Load the dotBW ground truth board matching an image path (same file name, ".txt" extension).
Board loadExpectedBoard(const std::filesystem::path& imagePath) {
	Board expected(0u);
	std::filesystem::path txtPath = imagePath;
	txtPath.replace_extension(".txt");
	EXPECT_TRUE(readBoard(txtPath, expected)) << txtPath.string();
	return expected;
}

//! Check every board coordinate (not just aggregate counts) against a ground truth board.
void expectStonesMatchBoard(const std::vector<StoneState>& stones, unsigned boardSize, const Board& expected) {
	ASSERT_EQ(stones.size(), static_cast<std::size_t>(boardSize) * boardSize);
	ASSERT_EQ(expected.size(), boardSize);

	for (unsigned x = 0; x < boardSize; ++x) {
		for (unsigned y = 0; y < boardSize; ++y) {
			const std::size_t index = static_cast<std::size_t>(x) * boardSize + y;
			EXPECT_EQ(stones[index], toStoneState(expected.get({x, y}))) << "Mismatch at (" << x << ", " << y << ")";
		}
	}
}

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
		EXPECT_EQ(stoneCount(result.stoneStep.stones), i);
		EXPECT_EQ(blackStoneCount(result.stoneStep.stones), std::floor(static_cast<double>(i) / 2.));
		EXPECT_EQ(whiteStoneCount(result.stoneStep.stones), std::ceil(static_cast<double>(i) / 2.));

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
		EXPECT_EQ(stoneCount(result.stoneStep.stones), i);
		EXPECT_EQ(blackStoneCount(result.stoneStep.stones), std::ceil(static_cast<double>(i) / 2.));
		EXPECT_EQ(whiteStoneCount(result.stoneStep.stones), std::floor(static_cast<double>(i) / 2.));

		const Board expected = loadExpectedBoard(TEST_PATH / fileName);
		expectStonesMatchBoard(result.stoneStep.stones, BOARD_SIZE, expected);
	}
}

// TODO: Add stone finder for angled_hard
TEST(Process, Board_Detect_Easy) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "angled_easy";

	static constexpr unsigned IMG_COUNT  = 6u;
	static constexpr unsigned BOARD_SIZE = 13u;

	// All angle images show the same physical board, so they share one ground truth file.
	Board expected(0u);
	ASSERT_TRUE(readBoard(TEST_PATH / "board.txt", expected));

	for (unsigned i = 1u; i <= IMG_COUNT; ++i) {
		std::string fileName = std::format("angle_{}.jpeg", i);
		TestResult result    = runPipeline(TEST_PATH / fileName);

		EXPECT_EQ(result.rectified.geometry.boardSize, BOARD_SIZE);
		// EXPECT_NEAR(result.rectified.geometry.spacing, SPACING, SPACING * 0.1); // Allow 5% deviation from expected spacing.

		EXPECT_TRUE(result.stoneStep.success);
		expectStonesMatchBoard(result.stoneStep.stones, BOARD_SIZE, expected);
	}
}

} // namespace gtest
} // namespace tengen::vision::core
