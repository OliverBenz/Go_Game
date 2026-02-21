#include "camera/rectifier.hpp"

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
		EXPECT_FALSE(warpResult.image.empty());
		EXPECT_FALSE(warpResult.H.empty());

		const auto geometry = rectifyImage(image, warpResult);
		EXPECT_FALSE(geometry.image.empty());
		EXPECT_FALSE(geometry.H.empty());
		EXPECT_EQ(geometry.intersections.size(), BOARD_SIZE * BOARD_SIZE);
		EXPECT_EQ(geometry.boardSize, BOARD_SIZE);
	}
}

TEST(Process, Find_Board_Easy) {
	runTest("angled_easy");
}

// TODO: These do not work yet.
TEST(Process, Find_Board_Hard) {
	runTest("angled_hard");
}

} // namespace gtest
} // namespace tengen::vision::core
