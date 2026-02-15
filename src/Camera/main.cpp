#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <iostream>

#include <opencv2/opencv.hpp>

#include "camera/rectifier.hpp"
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

// Example enum for output
enum class StoneState { Empty, Black, White };

// Helper: clamp ROI safely
static cv::Rect clampRect(const cv::Rect& r, const cv::Size& sz) {
    cv::Rect imgRect(0, 0, sz.width, sz.height);
    return r & imgRect;
}

void analyseBoard(const cv::Mat& image, go::camera::DebugVisualizer* debugger) {	
	// Rectify image and extract board geometry.
	BoardGeometry geometry = go::camera::rectifyImage(image, debugger);
	if (geometry.image.empty()) {
		std::cerr << "Failed to rectify image\n";
		return;
	}
	if (debugger){
		debugger->beginStage("Stone Detection");
	}

	// Convert to grayscale and denoise for stable sampling.
	cv::Mat gray;
	if (geometry.image.channels() == 1) {
		gray = geometry.image;
	} else if (geometry.image.channels() == 3) {
		cv::cvtColor(geometry.image, gray, cv::COLOR_BGR2GRAY);
	} else if (geometry.image.channels() == 4) {
		cv::cvtColor(geometry.image, gray, cv::COLOR_BGRA2GRAY);
	} else {
		std::cerr << "Unexpected channel count in rectified image: " << geometry.image.channels() << "\n";
		if (debugger) debugger->endStage();
		return;
	}

	cv::Mat blurred;
	cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.0);
	
	// Validate spacing and compute sampling radius (in pixels).
	if (!std::isfinite(geometry.spacing) || geometry.spacing < 4.0) {
		std::cerr << "Invalid grid spacing for stone detection: " << geometry.spacing << "\n";
		if (debugger) debugger->endStage();
		return;
	}

	const int maxRadiusPx = std::max(2, (std::min(blurred.cols, blurred.rows) - 1) / 2);
	int roiRadiusPx = (int)std::lround(0.45 * geometry.spacing);
	roiRadiusPx = std::max(2, std::min(roiRadiusPx, maxRadiusPx));
	const int patchSize = 2 * roiRadiusPx + 1;

	// Sample circular ROIs at each intersection (mean intensity).
	cv::Mat circleMask(patchSize, patchSize, CV_8U, cv::Scalar(0));
	cv::circle(circleMask, cv::Point(roiRadiusPx, roiRadiusPx), roiRadiusPx, cv::Scalar(255), -1, cv::LINE_8);

	std::vector<float> intensities(geometry.intersections.size(), 0.0f);
	std::vector<int> validIdx;
	validIdx.reserve(geometry.intersections.size());

	for (std::size_t idx = 0; idx < geometry.intersections.size(); ++idx) {
		const cv::Point2f p = geometry.intersections[idx];
		if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
		const int cx = (int)std::lround(p.x);
		const int cy = (int)std::lround(p.y);

		const cv::Rect roi0(cx - roiRadiusPx, cy - roiRadiusPx, patchSize, patchSize);
		const cv::Rect roi = clampRect(roi0, blurred.size());
		if (roi.width <= 0 || roi.height <= 0) {
			continue;
		}

		const cv::Mat patch = blurred(roi);
		const cv::Rect maskRect(roi.x - roi0.x, roi.y - roi0.y, roi.width, roi.height);
		if (maskRect.x < 0 || maskRect.y < 0 || maskRect.x + maskRect.width > circleMask.cols ||
		    maskRect.y + maskRect.height > circleMask.rows || maskRect.size() != patch.size()) {
			continue;
		}

		const cv::Mat maskROI = circleMask(maskRect);

		cv::Scalar mean, stddev;
		cv::meanStdDev(patch, mean, stddev, maskROI);
		const float m = static_cast<float>(mean[0]);
		if (!std::isfinite(m)) {
			continue;
		}

		intensities[idx] = m;
		validIdx.push_back((int)idx);
	}
	
	// Cluster intensities into black/empty/white via k-means (k=3).
	const int K = 3;
	cv::Mat labels, centers;
	bool haveClusters = false;
	if ((int)validIdx.size() >= K) {
		cv::Mat samples((int)validIdx.size(), 1, CV_32F);
		for (int i = 0; i < (int)validIdx.size(); ++i) {
			samples.at<float>(i, 0) = intensities[(std::size_t)validIdx[(std::size_t)i]];
		}

		try {
			cv::kmeans(samples, K, labels,
			           cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 50, 1e-3),
			           5, cv::KMEANS_PP_CENTERS, centers);
			haveClusters = (labels.rows == samples.rows && centers.rows == K && centers.cols == 1);
		} catch (const cv::Exception& e) {
			std::cerr << "kmeans failed: " << e.what() << "\n";
		}
	}

	// Map cluster labels to stone states (dark->black, bright->white, mid->empty).
	std::vector<StoneState> stateFlat(geometry.intersections.size(), StoneState::Empty);
	if (haveClusters) {
		std::array<int, K> order{0, 1, 2};
		std::sort(order.begin(), order.end(), [&](int a, int b) {
			return centers.at<float>(a, 0) < centers.at<float>(b, 0);
		});

		const int blackCluster = order[0];
		const int emptyCluster = order[1];
		const int whiteCluster = order[2];

		std::array<StoneState, K> clusterToState{StoneState::Empty, StoneState::Empty, StoneState::Empty};
		clusterToState[blackCluster] = StoneState::Black;
		clusterToState[emptyCluster] = StoneState::Empty;
		clusterToState[whiteCluster] = StoneState::White;

		for (int i = 0; i < labels.rows; ++i) {
			const int lab = labels.at<int>(i, 0);
			if (lab < 0 || lab >= K) continue;
			stateFlat[(std::size_t)validIdx[(std::size_t)i]] = clusterToState[lab];
		}
	}

	// Build a clean overlay image of detected stones on the refined board.
	if (debugger) {
		cv::Mat vis;
		if (geometry.image.channels() == 1) cv::cvtColor(geometry.image, vis, cv::COLOR_GRAY2BGR);
		else if (geometry.image.channels() == 3) vis = geometry.image.clone();
		else if (geometry.image.channels() == 4) cv::cvtColor(geometry.image, vis, cv::COLOR_BGRA2BGR);
		else vis = geometry.image.clone();

		const int drawRadiusPx = roiRadiusPx;
		const int thickness = std::max(1, (int)std::lround(drawRadiusPx * 0.12));

		for (std::size_t idx = 0; idx < geometry.intersections.size(); ++idx) {
			const auto& p = geometry.intersections[idx];
			if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;

			if (stateFlat[idx] == StoneState::Black) {
				cv::circle(vis, p, drawRadiusPx, cv::Scalar(0, 0, 0), thickness, cv::LINE_AA);
			} else if (stateFlat[idx] == StoneState::White) {
				cv::circle(vis, p, drawRadiusPx, cv::Scalar(255, 0, 0), thickness, cv::LINE_AA); // blue (BGR)
			}
		}

		debugger->add("Detected Stones", vis);
		debugger->endStage();
	}

}

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

        go::camera::BoardGeometry result = go::camera::rectifyImage(image, &debug);
		if (!result.image.empty()) {
            cv::imshow("Rectified", result.image);
        }

        cv::Mat mosaic = debug.buildMosaic();
        if (!mosaic.empty()) {
            cv::imshow("Debug Mosaic", mosaic);
        }

        cv::waitKey(0);
	} else {
		// Load Image
		cv::Mat image9  = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_9/move_13.png");
		// cv::Mat image13 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "easy_straight/size_13.jpeg");
		// cv::Mat image19 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "easy_straight/size_19.jpeg");
		
		go::camera::analyseBoard(image9, &debug);
		//auto rect13 = go::camera::rectifyImage(image13);
		//auto rect19 = go::camera::rectifyImage(image19);

		// showImages(rect9, rect13, rect19);

		const auto mosaic = debug.buildMosaic();
		//cv::imshow("", mosaic);
		//cv::waitKey(0);
		cv::imwrite("/home/oliver/temp.png", mosaic);
		//analyseBoard(image);
	}

	return 0;
}
