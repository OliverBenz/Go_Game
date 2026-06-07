#include "syntheticBoard.hpp"

#include "vision/core/gridFinder.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

namespace tengen::vision::core {
namespace gtest {

// Check that the board can be detected. Same board different angles.
void runTest(const std::string& testSetName) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / testSetName;

	static constexpr unsigned IMG_COUNT  = 6u;
	static constexpr unsigned BOARD_SIZE = 13u;

	for (unsigned i = 1u; i <= IMG_COUNT; ++i) {
		std::string fileName = std::format("angle_{}.jpeg", i);

		cv::Mat image = cv::imread(TEST_PATH / fileName);
		ASSERT_FALSE(image.empty());

		const auto warpResult = warpToBoard(image);
		EXPECT_FALSE(warpResult.imageB0.empty());
		EXPECT_FALSE(warpResult.H0.empty());

		const auto geometry  = analyseGeometry(warpResult);
		const auto rectified = transformImage(image, geometry);
		EXPECT_FALSE(rectified.imageB.empty());
		EXPECT_FALSE(rectified.geometry.H.empty());
		EXPECT_EQ(rectified.geometry.intersections.size(), BOARD_SIZE * BOARD_SIZE);
		EXPECT_EQ(rectified.geometry.boardSize, BOARD_SIZE);
	}
}

TEST(Process, Find_Board_Easy) {
	runTest("angled_easy");
}

// TODO: These do not work yet.
TEST(Process, Find_Board_Hard) {
	runTest("angled_hard");
}

TEST(Process, Find_Board_Synthetic_FullFramePerspective) {
	const cv::Mat image = makeFullFrameSyntheticScene();
	ASSERT_FALSE(image.empty());

	const auto warpResult = warpToBoard(image);
	EXPECT_TRUE(isValidBoard(warpResult));

	const auto geometry  = analyseGeometry(warpResult);
	const auto rectified = transformImage(image, geometry);
	EXPECT_FALSE(rectified.imageB.empty());
	EXPECT_FALSE(rectified.geometry.H.empty());
	EXPECT_EQ(rectified.geometry.boardSize, 13u);
}

TEST(Process, Find_Board_Synthetic_OutlineOnly) {
	const cv::Mat image = makeOutlineOnlySyntheticScene();
	ASSERT_FALSE(image.empty());

	const auto warpResult = warpToBoard(image);
	EXPECT_TRUE(isValidBoard(warpResult));
}

} // namespace gtest
} // namespace tengen::vision::core
