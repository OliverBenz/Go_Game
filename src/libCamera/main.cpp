#include "BoardDetect.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

static constexpr bool kUseLiveCamera = true;
static constexpr int kCameraIndex    = 0;

int hmin = 0, smin = 89, vmin = 178;
int hmax = 161, smax = 220, vmax = 226;

//! Uses default HSV space that manually was found to provide a good contour
//! of the board and stones in simple imaging setups.
class MaskContour {
public:
	// Apply HSV mask to one image
	cv::Mat process(const cv::Mat& img) {
		cv::Mat hsv, mask, result;
		cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
		cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);
		cv::cvtColor(mask, result, cv::COLOR_GRAY2BGR); // so we can stack consistently
		return result;
	}

private:
	static constexpr int hmin = 13, smin = 44, vmin = 194;
	static constexpr int hmax = 37, smax = 197, vmax = 255;

	//! Shifting vmin around the base by this amound shoud allow to find most boards.
	static constexpr int vminRange = 20;
};

class MaskTesterHSV {
public:
	MaskTesterHSV() {
		// Create trackbars
		namedWindow("Mask", cv::WINDOW_AUTOSIZE);
		cv::createTrackbar("H Min", "Mask", &hmin, 179);
		cv::createTrackbar("H Max", "Mask", &hmax, 179);
		cv::createTrackbar("S Min", "Mask", &smin, 255);
		cv::createTrackbar("S Max", "Mask", &smax, 255);
		cv::createTrackbar("V Min", "Mask", &vmin, 255);
		cv::createTrackbar("V Max", "Mask", &vmax, 255);
	}
	// Apply HSV mask to one image
	cv::Mat processSimpleMask(const cv::Mat& img) {
		cv::Mat hsv, mask, result;
		cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
		cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);
		cv::cvtColor(mask, result, cv::COLOR_GRAY2BGR); // so we can stack consistently
		return result;
	}

	cv::Mat process(const cv::Mat& img) {
		cv::Mat hsv, mask;
		cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
		cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);

		// --- find board contour ---
		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		if (contours.empty()) {
			return img.clone(); // fallback
		}

		// largest contour = board
		double maxArea = 0;
		int maxIdx     = -1;
		for (size_t i = 0; i < contours.size(); i++) {
			double area = cv::contourArea(contours[i]);
			if (area > maxArea) {
				maxArea = area;
				maxIdx  = (int)i;
			}
		}

		std::vector<cv::Point> approx;
		cv::approxPolyDP(contours[maxIdx], approx, 0.02 * cv::arcLength(contours[maxIdx], true), true);

		if (approx.size() != 4) {
			// not a quad, just show mask overlay
			cv::Mat debug;
			cv::cvtColor(mask, debug, cv::COLOR_GRAY2BGR);
			cv::drawContours(debug, contours, maxIdx, cv::Scalar(0, 255, 0), 2);
			return debug;
		}

		// --- order the 4 corners consistently ---
		// compute centroid
		cv::Point2f center(0, 0);
		for (auto& p: approx)
			center += cv::Point2f(p);
		center *= (1.0 / approx.size());

		std::vector<cv::Point2f> ordered(4);
		for (auto& p: approx) {
			if (p.x < center.x && p.y < center.y)
				ordered[0] = p; // top-left
			else if (p.x > center.x && p.y < center.y)
				ordered[1] = p; // top-right
			else if (p.x > center.x && p.y > center.y)
				ordered[2] = p; // bottom-right
			else
				ordered[3] = p; // bottom-left
		}

		// --- target rectangle size ---
		int outSize                     = 800; // you can pick board resolution
		std::vector<cv::Point2f> target = {{0, 0}, {outSize - 1, 0}, {outSize - 1, outSize - 1}, {0, outSize - 1}};

		// --- perspective transform ---
		cv::Mat M = cv::getPerspectiveTransform(ordered, target);
		cv::Mat warped;
		cv::warpPerspective(img, warped, M, cv::Size(outSize, outSize));

		return warped;
	}


	// TODO: Stone detection is bad. I need a different mask to find stones and their colour.
	cv::Mat processWithStones(const cv::Mat& img) {
		cv::Mat warped     = process(img);
		auto stones        = detectStones(warped);
		cv::Mat withStones = drawStones(warped, stones);

		return withStones;
	}

	std::vector<cv::Vec3f> detectStones(const cv::Mat& warpedBoard) {
		cv::Mat gray;
		cv::cvtColor(warpedBoard, gray, cv::COLOR_BGR2GRAY);
		cv::medianBlur(gray, gray, 5);

		std::vector<cv::Vec3f> circles;
		cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1,
		                 20,               // minDist between stone centers (tune: ~cell size)
		                 100, 30, 10, 30); // param1, param2, minRadius, maxRadius

		return circles;
	}

	cv::Mat drawStones(const cv::Mat& img, const std::vector<cv::Vec3f>& stones) {
		cv::Mat result = img.clone();
		for (size_t i = 0; i < stones.size(); i++) {
			cv::Point center(cvRound(stones[i][0]), cvRound(stones[i][1]));
			int radius = cvRound(stones[i][2]);
			// Circle outline
			cv::circle(result, center, radius, cv::Scalar(0, 255, 0), 2);
			// Center dot
			cv::circle(result, center, 2, cv::Scalar(0, 0, 255), 3);
		}
		return result;
	}


	int hmin = 13, smin = 44, vmin = 187; // or vmin 186
	int hmax = 37, smax = 197, vmax = 255;
};

bool supportsHighGui() {
	try {
		cv::namedWindow("Probe", cv::WINDOW_AUTOSIZE);
		cv::destroyWindow("Probe");
		return true;
	} catch (const cv::Exception&) { return false; }
}

cv::Mat makeGrid(const std::vector<cv::Mat>& images) {
	if (images.size() != 6) {
		std::cerr << "Need exactly 6 images\n";
		return cv::Mat();
	}

	// Resize all to the same size for neat layout
	cv::Size sz(320, 240); // pick a display size
	std::vector<cv::Mat> resized;
	for (auto& img: images) {
		cv::Mat r;
		cv::resize(img, r, sz);
		resized.push_back(r);
	}

	// Row 1
	cv::Mat row1;
	cv::hconcat(std::vector<cv::Mat>{resized[0], resized[1], resized[2]}, row1);

	// Row 2
	cv::Mat row2;
	cv::hconcat(std::vector<cv::Mat>{resized[3], resized[4], resized[5]}, row2);

	// Final grid
	cv::Mat grid;
	cv::vconcat(row1, row2, grid);

	return grid;
}

cv::Mat loadIfExists(const std::filesystem::path& path) {
	const auto img = cv::imread(path.string());
	return img.empty() ? cv::Mat() : img;
}

cv::Mat buildFinalView(const std::filesystem::path& debugDir) {
	const auto warped = loadIfExists(debugDir / "3_warped.png");
	if (warped.empty()) {
		return {};
	}

	cv::Mat combined = warped.clone();
	const auto contour = loadIfExists(debugDir / "2_boardContour.png");
	const auto lines   = loadIfExists(debugDir / "4_gridLines.png");
	auto stones        = loadIfExists(debugDir / "5_stones.png");
	if (stones.empty()) {
		stones = loadIfExists(debugDir / "5_stones_grid.png");
	}

	const auto overlay = [&](const cv::Mat& img) {
		if (img.empty()) {
			return;
		}
		if (img.size() == combined.size()) {
			cv::addWeighted(combined, 0.7, img, 0.3, 0.0, combined);
			return;
		}
		cv::Mat resized;
		cv::resize(img, resized, combined.size(), 0.0, 0.0, cv::INTER_NEAREST);
		cv::addWeighted(combined, 0.7, resized, 0.3, 0.0, combined);
	};

	overlay(contour);
	overlay(lines);
	overlay(stones);

	return combined;
}


int main() {
	const bool forceHeadless = std::getenv("GO_CAMERA_HEADLESS") != nullptr;
	const bool guiAvailable  = !forceHeadless && supportsHighGui();

	if (guiAvailable && kUseLiveCamera) {
		cv::VideoCapture cap(kCameraIndex);
		if (!cap.isOpened()) {
			std::cerr << "Failed to open camera index " << kCameraIndex << ".\n";
			return 1;
		}

		BoardDetect detector("camera_debug_live");
		while (true) {
			cv::Mat frame;
			cap >> frame;
			if (frame.empty()) {
				break;
			}

			detector.process(frame);
			cv::Mat view = buildFinalView("camera_debug_live");
			if (view.empty()) {
				view = frame;
			}
			cv::imshow("BoardDetect", view);

			if (cv::waitKey(1) == 27) {
				break; // Esc to quit
			}
		}
	} else {
		// Load your 6 reference images
		std::vector<cv::Mat> processed;
		for (auto i = 1; i != 7; ++i) {
			const auto imgPath = std::filesystem::path(PATH_TEST_IMG) / std::format("boards_easy/angle_{}.jpeg", i);
			cv::Mat img = cv::imread(imgPath);
			if (img.empty()) {
				std::cerr << "Could not read " << imgPath << std::endl;
				return -1;
			}

			const auto debugDir = std::filesystem::path("camera_debug_static") / ("angle_" + std::to_string(i));
			BoardDetect detector(debugDir.string());
			detector.process(img);
			const auto view = buildFinalView(debugDir);
			processed.push_back(view.empty() ? img : view);
		}

		const cv::Mat grid = makeGrid(processed);
		if (guiAvailable) {
			cv::imshow("BoardDetect", grid);
			cv::waitKey(0);
		} else {
			const auto outPath = std::filesystem::path("board_detect_grid.png");
			if (cv::imwrite(outPath.string(), grid)) {
				std::cout << "Saved " << outPath << " (GUI unavailable).\n";
			} else {
				std::cerr << "Failed to write " << outPath << ".\n";
			}
		}
	}

	return 0;
}
