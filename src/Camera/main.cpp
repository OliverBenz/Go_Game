#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>

#include <opencv2/opencv.hpp>

#include "camera/rectifier.hpp"
#include "camera/stoneFinder.hpp"
#include "gridFinder.hpp"

namespace go::camera {
// Notes and Findings:
// - Board Detection
//   - Easy Straight Dataset
//     - Adaptive Threshold:   Visually appears to work nicely. May conflict with background
//     - OTSU Threshold:       Suboptimal. May require further tuning.
//     - Canny Edge Detection: Visually appears to work. Further tuning needed.

// Tunable Parameters (Default values set below. Real application requires more (adaptive?) tuning):
// - Gaussian Blur:        (Size{5,5}, 1) results in weaker Canny result than (Size{7,7},1.5)
// - Canny Edge Detection: Check documentation. More parameters available.

// Board Detection
// 1) Coarse Detection (warpToBoard):  Warps the image to the board but not yet specific which exact board contour is found (outermost grid lines vs physical
// board contour). 2) Normalise        (warpToBoard):  Output image has fixed normalised size. 3) Refine           (rectifyImage): Border or image is the
// outermost grid lines + Tolerance for Stones placed at edge. 4) Re-Normalise     (rectifyImage): Final image normalised and with proper border setup.

void showImages(cv::Mat& image1, cv::Mat& image2, cv::Mat& image3) {
	double scale = 0.4; // adjust as needed
	cv::Mat small1, small2, small3;

	cv::resize(image1, small1, cv::Size(), scale, scale);
	cv::resize(image2, small2, cv::Size(), scale, scale);
	cv::resize(image3, small3, cv::Size(), scale, scale);

	// Stack horizontally
	cv::Mat combined;
	cv::hconcat(std::vector<cv::Mat>{small1, small2, small3}, combined);

	cv::imshow("3 Images", combined);
	cv::waitKey(0);
}

static bool isValidBoardSize(unsigned size) {
	return size == 9 || size == 13 || size == 19;
}

// TODO: Better validity checks. Add success flag to all? Maybe return optionals?
bool process(const std::filesystem::path& path, DebugVisualizer* debugger = nullptr) {
	// Read image
	cv::Mat image = cv::imread(path.string());
	if (image.empty()) {
		std::cerr << "Failed to load image: " << path << "\n";
		return false;
	}

	// Warp image roughly around the board.
	WarpResult warped = warpToBoard(image, debugger);
	if (warped.image.empty() || warped.H.empty()) {
		std::cerr << "[Error] Could not find board in image.\n";
		return false;
	}

	// Properly construct the board geometry.
	BoardGeometry geometry = rectifyImage(image, warped, debugger);
	if (geometry.image.empty() || geometry.H.empty() || !isValidBoardSize(geometry.boardSize) ||
	    geometry.boardSize * geometry.boardSize != geometry.intersections.size()) {
		std::cerr << "[Error] Could not construct board geometry from warped image.\n";
		return false;
	}

	// Find the stones on the board.
	StoneResult result = analyseBoard(geometry, debugger);
	if (!result.success) {
		std::cerr << "[Error] Could not analyse the board to find stones.\n";
		return false;
	}

	return true;
}

} // namespace go::camera

// 3 steps
// 1) Find board in image and rectify (find largest plausible board contour, dont care if its physical board or outer grid contour)
// 2) Verify board size, find contours and adapt image again
//    - Cut image to outermost grid lines + Buffer for edge stones. Do not cut to physical board boundary.
//    - Use board size etc for testing
// --- HERE, we have a solid intermediate state. We do not have to repeat this every for frame of the video feed.
//     But only when the camera changes (would have to detect this)
// - Output: Board cropped + Board size. Expect stable
// 3) Detect grid lines again and stones.
int main(int argc, char** argv) {
	go::camera::DebugVisualizer debug;
	debug.setInteractive(false);

	// If a path is passed here then use this image. Else do test images.
	if (argc > 1) {
		std::filesystem::path inputPath = argv[1]; // Path from command line.

		if (go::camera::process(inputPath, &debug)) {
			cv::Mat mosaic = debug.buildMosaic();
			if (!mosaic.empty()) {
				cv::imshow("Debug Mosaic", mosaic);
			}
			cv::waitKey(0);
		}

	} else {
		const auto exampleImage = std::filesystem::path(PATH_TEST_IMG) / "angled_easy/angle_4.jpeg";

		if (go::camera::process(exampleImage, &debug)) {
			const auto mosaic = debug.buildMosaic();
			// cv::imshow("", mosaic);
			// cv::waitKey(0);
			cv::imwrite("/home/oliver/temp.png", mosaic);
		}
	}

	return 0;
}
