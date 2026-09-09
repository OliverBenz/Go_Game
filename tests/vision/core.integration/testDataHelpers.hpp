#pragma once

#include "model/board.hpp"
#include "vision/core/boardFinder.hpp"
#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

namespace tengen::vision::core {
namespace gtest {

//! Expected test results (result of each step in the pipeline).
struct TestResult {
	WarpResult warped;
	RectifiedBoard rectified;
	StoneResult stoneStep;
};

//! Run the stone detection pipeline. Ensure intermediate steps are generally valid. Return test result.
TestResult runPipeline(const std::filesystem::path& imgPath);

//! Load the dotBW ground truth board matching an image path (same file name, ".txt" extension).
Board loadExpectedBoard(const std::filesystem::path& imagePath);

//! Check every board coordinate  against a ground truth board.
void expectStonesMatchBoard(const std::vector<StoneState>& stones, unsigned boardSize, const Board& expected);


//! Manually labeled board geometry. All points are in the original image space.
struct GeometryGroundTruth {
	unsigned boardSize{};
	std::vector<cv::Point2f> boardCorners; //!< Outer edge of the physical board (4 points, unordered).
	std::vector<cv::Point2f> gridCorners;  //!< Outermost grid-line intersections (4 points, unordered).
};

//! Load the geometry ground truth matching an image path (same file name, ".json" extension).
GeometryGroundTruth loadGeometryGroundTruth(const std::filesystem::path& imagePath);

//! Match two equally-sized point sets without assuming a fixed order (a board photographed at a
//! strong angle has no well-defined "top-left" corner), then check every matched pair is within
//! tolerance. Brute-forces all permutations, which is fine for the small (<=4) point sets we use this for.
void expectPointsMatch(const std::vector<cv::Point2f>& expected, const std::vector<cv::Point2f>& actual, float tolerance, std::string_view context);

} // namespace gtest
} // namespace tengen::vision::core
