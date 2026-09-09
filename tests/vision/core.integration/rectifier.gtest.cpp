#include "vision/core/gridFinder.hpp"

#include <array>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

namespace tengen::vision::core {
namespace gtest {

// Check that simple boards (single image per board size) can be rectified and the board size is correctly detected.
void runRectifyTest(const std::string& testSetName) {
	const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / testSetName;

	static constexpr std::array<unsigned, 3> BOARD_SIZES = {9u, 13u, 19u};

	for (const unsigned boardSize: BOARD_SIZES) {
		std::string fileName = std::format("size_{}.jpeg", boardSize);

		cv::Mat image = cv::imread((TEST_PATH / fileName).string());
		ASSERT_FALSE(image.empty());

		const auto warpResult = warpToBoard(image);
		EXPECT_FALSE(warpResult.imageB0.empty());
		EXPECT_FALSE(warpResult.H0.empty());

		const auto geometry  = analyseGeometry(warpResult);
		const auto rectified = transformImage(image, geometry);
		EXPECT_FALSE(rectified.imageB.empty());
		EXPECT_FALSE(rectified.geometry.H.empty());
		EXPECT_EQ(rectified.geometry.intersections.size(), boardSize * boardSize);
		EXPECT_EQ(rectified.geometry.boardSize, boardSize);
	}
}

TEST(Rectifier, RectifyImage_Straight) {
	runRectifyTest("empty_angle_none");
}

TEST(Rectifier, RectifyImage_SmallAngle) {
	runRectifyTest("empty_angle_small");
}

} // namespace gtest
} // namespace tengen::vision::core
