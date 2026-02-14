#include <cstdlib>
#include <filesystem>
#include <vector>
#include <iostream>

#include <opencv2/opencv.hpp>

#include "camera/rectifier.hpp"
#include "statistics.hpp"
#include "gridFinder.hpp"

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
// 1) Coarse Detection (warpToBoard):  Warps the image to the board but not yet specific which exact board contour is found (outermost grid lines vs physical board contour).
// 2) Normalise        (warpToBoard):  Output image has fixed normalised size.
// 3) Refine           (rectifyImage): Border or image is the outermost grid lines + Tolerance for Stones placed at edge.
// 4) Re-Normalise     (rectifyImage): Final image normalised and with proper border setup.

void showImages(cv::Mat& image1, cv::Mat& image2, cv::Mat& image3) {
	double scale = 0.4;  // adjust as needed
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

void analyseBoard(const cv::Mat& image, DebugVisualizer* debugger) {	
	cv::Mat input = rectifyImage(image, debugger);
	if (image.empty()) {
		std::cerr << "Failed to rectify image\n";
		return;
	}
	if (debugger) {
		debugger->beginStage("Board Analyse");
		debugger->add("Input", input);
	}

	// 1. Grayscale
	cv::Mat gray;
	cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

	// 2. Gaussian Blur to reduce noise
	cv::Mat blurred;
	cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.5);
	
	
	// 3. Otsu Threshold
    cv::Mat otsu;
    cv::threshold(blurred, otsu, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

    // 4. Adaptive Threshold
    cv::Mat adaptive;
    cv::adaptiveThreshold(blurred, adaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 2);

    // 5. Canny
    cv::Mat canny;
    cv::Canny(blurred, canny, 50, 150);

    // 6. Stack horizontally
    cv::Mat combined;
    cv::hconcat(std::vector<cv::Mat>{otsu, adaptive, canny}, combined);

	showImages(otsu, adaptive, canny);
}

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
        // ---- Single image from command line ----
        std::filesystem::path inputPath = argv[1];

        cv::Mat image = cv::imread(inputPath.string());
        if (image.empty()) {
            std::cerr << "Failed to load image: " << inputPath << "\n";
            return 1;
        }

        auto rectified = go::camera::rectifyImage(image, &debug);
		if (!rectified.empty()) {
            cv::imshow("Rectified", rectified);
        }

        cv::Mat mosaic = debug.buildMosaic();
        if (!mosaic.empty()) {
            cv::imshow("Debug Mosaic", mosaic);
        }

        cv::waitKey(0);
	} else {
		// Load Image
		cv::Mat image9  = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "easy_straight/size_9.jpeg");
		cv::Mat image13 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "easy_straight/size_13.jpeg");
		cv::Mat image19 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "easy_straight/size_19.jpeg");
		
		auto rect9  = go::camera::rectifyImage(image9);
		auto rect13 = go::camera::rectifyImage(image13, &debug);
		auto rect19 = go::camera::rectifyImage(image19);

		// showImages(rect9, rect13, rect19);

		const auto mosaic = debug.buildMosaic();
		//cv::imshow("", mosaic);
		//cv::waitKey(0);
		cv::imwrite("/home/oliver/temp.png", mosaic);
		//analyseBoard(image);
	}

	return 0;
}
