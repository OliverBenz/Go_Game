#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
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

// Stone detection on a rectified Go board.
// Pipeline: rectify -> grayscale/blur -> per-intersection circular mean intensity -> kmeans(K=3) -> validate clusters -> overlay.
// Key caveat: kmeans *always* partitions the data (even on an empty board), so we gate “stone clusters” by spread/support/separation.
void analyseBoard(const cv::Mat& image, go::camera::DebugVisualizer* debugger) {	
	// Step 0: Rectify the board and get geometry (top-down image + intersections + spacing in refined coordinates).
	BoardGeometry geometry = go::camera::rectifyImage(image, debugger);
	if (geometry.image.empty()) {
		std::cerr << "Failed to rectify image\n";
		return;
	}
	if (debugger) {
		debugger->beginStage("Stone Detection");
		debugger->add("Input", geometry.image);
	}

	// Step 1: Convert to grayscale (intensity-only classifier) and denoise to stabilize ROI (Region of Interest) means.
	cv::Mat gray;
	cv::cvtColor(geometry.image, gray, cv::COLOR_BGR2GRAY);
	if (debugger) debugger->add("Grayscale", gray);

	cv::Mat imgBlurred;
	cv::GaussianBlur(gray, imgBlurred, cv::Size(5, 5), 1.0);
	if (debugger) debugger->add("Gaussian Blur", imgBlurred);

	// Step 2: Convert board geometry spacing into a robust sampling radius (pixels).
	if (!std::isfinite(geometry.spacing) || geometry.spacing < 4.0) {
		std::cerr << "Invalid grid spacing for stone detection: " << geometry.spacing << "\n";
		if (debugger) debugger->endStage();
		return;
	}

	const int maxRadiusPx = std::max(2, (std::min(imgBlurred.cols, imgBlurred.rows) - 1) / 2);
	int roiRadiusPx = (int)std::lround(0.45 * geometry.spacing); // ~stone radius fraction of grid spacing (tune per dataset).
	roiRadiusPx = std::max(2, std::min(roiRadiusPx, maxRadiusPx));
	const int patchSize = 2 * roiRadiusPx + 1;
	assert(patchSize > 0);

	// Step 3: Build a circular mask once; we’ll crop it to match border-clipped ROIs.
	cv::Mat circleMask(patchSize, patchSize, CV_8U, cv::Scalar(0));
	cv::circle(circleMask, cv::Point(roiRadiusPx, roiRadiusPx), roiRadiusPx, cv::Scalar(255), -1, cv::LINE_8);

	// Step 4: For each intersection, sample the mean grayscale value inside the circular ROI.
	std::vector<float> intensities(geometry.intersections.size(), 0.0f);
	std::vector<int> validIdx;
	validIdx.reserve(geometry.intersections.size());

	for (std::size_t idx = 0; idx < geometry.intersections.size(); ++idx) {
		const cv::Point2f p = geometry.intersections[idx];
		if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;
		const int cx = (int)std::lround(p.x); // intersection center x (pixels)
		const int cy = (int)std::lround(p.y); // intersection center y (pixels)

		// ROI around the intersection; clampRect avoids OpenCV asserts at image borders.
		const cv::Rect roi0(cx - roiRadiusPx, cy - roiRadiusPx, patchSize, patchSize);
		const cv::Rect roi = clampRect(roi0, imgBlurred.size());
		if (roi.width <= 0 || roi.height <= 0) {
			continue;
		}

		const cv::Mat patch = imgBlurred(roi);

		// maskRect maps the clamped ROI back into the full mask coordinate system (handles border clipping).
		const cv::Rect maskRect(roi.x - roi0.x, roi.y - roi0.y, roi.width, roi.height);
		if (maskRect.x < 0 || maskRect.y < 0 || maskRect.x + maskRect.width > circleMask.cols ||
		    maskRect.y + maskRect.height > circleMask.rows || maskRect.size() != patch.size()) {
			continue;
		}

		const cv::Mat maskROI = circleMask(maskRect);

		// meanStdDev with mask computes stats on masked pixels only (here: circular area).
		cv::Scalar mean, stddev;
		cv::meanStdDev(patch, mean, stddev, maskROI);
		const float m = static_cast<float>(mean[0]);
		if (!std::isfinite(m)) {
			continue;
		}

		intensities[idx] = m;
		validIdx.push_back((int)idx);
	}

	// Debug: visualize sampled intensities at valid intersections (per-point heatmap overlay).
	if (debugger) {
		cv::Mat vis;
		cv::cvtColor(gray, vis, cv::COLOR_GRAY2BGR);

		// Use a fixed, plausible display range centered at the median so we don't stretch small lighting gradients.
		std::vector<double> vals;
		vals.reserve(validIdx.size());
		for (int idx : validIdx) {
			const double v = intensities[(std::size_t)idx];
			if (std::isfinite(v)) vals.push_back(v);
		}

		double midI = 127.5;
		if (!vals.empty()) {
			auto midIt = vals.begin() + (vals.size() / 2);
			std::nth_element(vals.begin(), midIt, vals.end());
			midI = *midIt;
		}

		const double kHeatmapHalfRange = 60.0; // +/- around median (in gray levels).
		double lowI = std::max(0.0, midI - kHeatmapHalfRange);
		double highI = std::min(255.0, midI + kHeatmapHalfRange);
		if (!std::isfinite(lowI) || !std::isfinite(highI) || highI <= lowI + 1.0) {
			lowI = 0.0;
			highI = 255.0;
		}

		// Build a 0..255 -> BGR lookup table once, then index into it per point.
		cv::Mat ramp(256, 1, CV_8UC1);
		for (int i = 0; i < 256; ++i) ramp.at<std::uint8_t>(i, 0) = (std::uint8_t)i;
		cv::Mat rampBgr;
		cv::applyColorMap(ramp, rampBgr, cv::COLORMAP_TURBO);

		const double denom = highI - lowI;
		const int r = std::max(1, roiRadiusPx / 3);
		for (int idx : validIdx) {
			const cv::Point2f p = geometry.intersections[(std::size_t)idx];
			if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;

			const double v = intensities[(std::size_t)idx];
			if (!std::isfinite(v)) continue;

			int v8 = (int)std::lround(255.0 * (v - lowI) / denom);
			v8 = std::max(0, std::min(255, v8));

			const cv::Vec3b c = rampBgr.at<cv::Vec3b>(v8, 0);
			cv::circle(vis, p, r, cv::Scalar(c[0], c[1], c[2]), -1, cv::LINE_AA);
		}

		debugger->add("Intersection Intensities (median±60)", vis);
	}
		

	// Circle in real image
	// if (debugger) {
	// 	cv::Mat vis;
	// 	if (geometry.image.channels() == 1) cv::cvtColor(geometry.image, vis, cv::COLOR_GRAY2BGR);
	// 	else if (geometry.image.channels() == 3) vis = geometry.image.clone();
	// 	else if (geometry.image.channels() == 4) cv::cvtColor(geometry.image, vis, cv::COLOR_BGRA2BGR);
	// 	else vis = geometry.image.clone();

	// 	const int drawRadiusPx = roiRadiusPx;
	// 	const int thickness = std::max(1, (int)std::lround(drawRadiusPx * 0.12)); // scale thickness with radius for readability.

	// 	for (std::size_t idx = 0; idx < geometry.intersections.size(); ++idx) {
	// 		const auto& p = geometry.intersections[idx];
	// 		if (!std::isfinite(p.x) || !std::isfinite(p.y)) continue;

	// 		if (stateFlat[idx] == StoneState::Black) {
	// 			cv::circle(vis, p, drawRadiusPx, cv::Scalar(0, 0, 0), thickness, cv::LINE_AA); // black outline
	// 		} else if (stateFlat[idx] == StoneState::White) {
	// 			cv::circle(vis, p, drawRadiusPx, cv::Scalar(255, 0, 0), thickness, cv::LINE_AA); // blue outline (BGR)
	// 		}
	// 	}

	// 	debugger->add("Detected Stones", vis);
	// 	debugger->endStage();
	// }

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
		cv::Mat image9  = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "game_simple/size_9/move_4.png");
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
