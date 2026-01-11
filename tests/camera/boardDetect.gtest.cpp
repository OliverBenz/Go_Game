#include "BoardDetect.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <opencv2/opencv.hpp>
#include <unordered_set>

namespace go::gtest {

TEST(BoardDetect, RejectsEmptyImage) {
	BoardDetect detector("camera_debug_test/empty");
	cv::Mat empty;
	const auto stones = detector.process(empty);
	EXPECT_TRUE(stones.empty());
}

TEST(BoardDetect, ProcessesEasyBoard) {
	const auto imgPath = std::filesystem::path(PATH_TEST_IMG) / "boards_easy/angle_2.jpeg";
	const auto img     = cv::imread(imgPath.string());
	ASSERT_FALSE(img.empty());

	const auto outDir = std::filesystem::path("camera_debug_test/easy_board");
	if (std::filesystem::exists(outDir)) {
		std::filesystem::remove_all(outDir);
	}

	BoardDetect detector(outDir.string());
	const auto stones = detector.process(img);

	EXPECT_EQ(detector.boardSize(), 13u);
	EXPECT_TRUE(std::filesystem::exists(outDir / "0_original.png"));
	EXPECT_TRUE(std::filesystem::exists(outDir / "1_boardMask.png"));
	EXPECT_TRUE(std::filesystem::exists(outDir / "2_boardContour.png"));
	EXPECT_TRUE(std::filesystem::exists(outDir / "3_warped.png"));
	EXPECT_TRUE(std::filesystem::exists(outDir / "4_gridLines.png"));
	EXPECT_TRUE(std::filesystem::exists(outDir / "5_stones.png"));

	// NOTE: We expect a non-empty stone set on this image.
	EXPECT_FALSE(stones.empty());
	EXPECT_LE(stones.size(), detector.boardSize() * detector.boardSize());

	// Ensure we only return unique board indices.
	std::unordered_set<unsigned> indices;
	for (const auto& stone : stones) {
		indices.insert(stone.first);
	}
	EXPECT_EQ(indices.size(), stones.size());
}

TEST(BoardDetect, ProcessesEasyBoardAngle1) {
	const auto imgPath = std::filesystem::path(PATH_TEST_IMG) / "boards_easy/angle_1.jpeg";
	const auto img     = cv::imread(imgPath.string());
	ASSERT_FALSE(img.empty());

	const auto outDir = std::filesystem::path("camera_debug_test/easy_board_angle1");
	if (std::filesystem::exists(outDir)) {
		std::filesystem::remove_all(outDir);
	}

	BoardDetect detector(outDir.string());
	const auto stones = detector.process(img);

	EXPECT_EQ(detector.boardSize(), 13u);
	EXPECT_FALSE(stones.empty());
	EXPECT_LE(stones.size(), detector.boardSize() * detector.boardSize());

	int blackCount = 0;
	int whiteCount = 0;
	for (const auto& stone : stones) {
		if (stone.second) {
			++blackCount;
		} else {
			++whiteCount;
		}
	}
	EXPECT_EQ(blackCount, 5);
	EXPECT_EQ(whiteCount, 5);
}

} // namespace go::gtest
