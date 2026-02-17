#include "camera/rectifier.hpp"
#include "camera/boardFinder.hpp"
#include "camera/stoneFinder.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <filesystem>

namespace go::camera::gtest {

//! Expected test results (result of each step in the pipeline).
struct TestResult {
    WarpResult warped;
    BoardGeometry geometry;
    StoneResult stoneStep;
};

//! Run the stone detection pipeline. Ensure intermediate steps are generally valid. Return test result.
TestResult runPipeline(const std::filesystem::path& imgPath) {
	cv::Mat image = cv::imread(imgPath.string());
    EXPECT_FALSE(image.empty());

	// Warp image roughly around the board.
	WarpResult warped = warpToBoard(image);
    EXPECT_FALSE(warped.image.empty());
    EXPECT_FALSE(warped.H.empty());

	// Properly construct the board geometry.
	BoardGeometry geometry = rectifyImage(image, warped);
    EXPECT_FALSE(geometry.image.empty());
    EXPECT_FALSE(geometry.H.empty());
    EXPECT_FALSE(geometry.intersections.empty());
    EXPECT_TRUE(geometry.intersections.size() == geometry.boardSize * geometry.boardSize);
    EXPECT_TRUE(geometry.boardSize == 9 || geometry.boardSize == 13 || geometry.boardSize == 19);
	
	// Find the stones on the board.
	StoneResult stoneRes = analyseBoard(geometry);
    EXPECT_TRUE(stoneRes.success);

    return {warped, geometry, stoneRes};
}

// Test the full image processing pipeline with stone detection at the end.
TEST(Process, Game_Simple_Size9) {
    const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_9";

    // Game Information    
    static constexpr unsigned MOVES      = 13;  //!< This game image series has 13 moves (+ a captures image). 
    static constexpr double SPACING      = 76.; //!< Pixels between grid lines. Manually checked for this series.
    static constexpr unsigned BOARD_SIZE = 9u;  //!< Board size of this game.

    for(unsigned i = 0; i <= MOVES; ++i) {
        std::string fileName = std::format("move_{}.png", i);
        TestResult result = runPipeline(TEST_PATH / fileName);

        EXPECT_EQ(result.geometry.boardSize, BOARD_SIZE);
        //EXPECT_NEAR(result.geometry.spacing, SPACING, SPACING * 0.1); // Allow 5% deviation from expected spacing.

        EXPECT_TRUE(result.stoneStep.success);
        EXPECT_EQ(result.stoneStep.stones.size(), i);

        // TODO: Check number of black&white stones and coordinates
    }

    TestResult result = runPipeline(TEST_PATH / "move_13_captured.png"); // One stone captured.
    EXPECT_TRUE(result.stoneStep.success);
    EXPECT_EQ(result.stoneStep.stones.size(), 12);
    // TODO: Check number of black&white stones and coordinates
}

// Test the full image processing pipeline with stone detection at the end.
TEST(Process, Game_Simple_Size13) {
    const auto TEST_PATH = std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_13";

    // Game Information    
    static constexpr unsigned MOVES      = 27;  //!< This game image series has 27 moves. 
    static constexpr double SPACING      = 72.; //!< Pixels between grid lines. Manually checked for this series.
    static constexpr unsigned BOARD_SIZE = 13u;  //!< Board size of this game.

    for(unsigned i = 0; i <= MOVES; ++i) {
        std::string fileName = std::format("move_{}.png", i);
        TestResult result = runPipeline(TEST_PATH / fileName);

        EXPECT_EQ(result.geometry.boardSize, BOARD_SIZE);
        //EXPECT_NEAR(result.geometry.spacing, SPACING, SPACING * 0.1); // Allow 5% deviation from expected spacing.

        EXPECT_TRUE(result.stoneStep.success);
        EXPECT_EQ(result.stoneStep.stones.size(), i);

        // TODO: Check number of black&white stones and coordinates
    }
    // TODO: Check number of black&white stones and coordinates
}

}