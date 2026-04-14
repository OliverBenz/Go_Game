#include "vision/devtools/syntheticBoard.hpp"

#include "vision/core/gridFinder.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

namespace tengen::vision::core {
namespace gtest {

TEST(Process, Find_Board_Synthetic_FullFramePerspective) {
	const cv::Mat image = devtools::makeFullFrameSyntheticScene();
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
	const cv::Mat image = devtools::makeOutlineOnlySyntheticScene();
	ASSERT_FALSE(image.empty());

	const auto warpResult = warpToBoard(image);
	EXPECT_TRUE(isValidBoard(warpResult));
}

} // namespace gtest
} // namespace tengen::vision::core
