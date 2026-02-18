#include "camera/boardFinder.hpp"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>


namespace go::camera {

//! Order 4 corner points TL,TR,BR,BL (Top Left, Bottom Right, etc).
static std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point>& quad) {
	CV_Assert(quad.size() == 4u); // Check before calling this function.

	std::vector<cv::Point2f> pts(4);
	for (int i = 0; i < 4; ++i) {
		pts[i] = quad[i];
	}

	// TL = min(x+y), BR = max(x+y)
	// TR = min(x-y), BL = max(x-y)
	std::vector<cv::Point2f> ordered(4);

	auto sumCmp  = [](const cv::Point2f& a, const cv::Point2f& b) { return (a.x + a.y) < (b.x + b.y); };
	auto diffCmp = [](const cv::Point2f& a, const cv::Point2f& b) { return (a.x - a.y) < (b.x - b.y); };

	ordered[0] = *std::min_element(pts.begin(), pts.end(), sumCmp);  // TL
	ordered[1] = *std::min_element(pts.begin(), pts.end(), diffCmp); // TR
	ordered[2] = *std::max_element(pts.begin(), pts.end(), sumCmp);  // BR
	ordered[3] = *std::max_element(pts.begin(), pts.end(), diffCmp); // BL

	return ordered;
}

// TODO: Add some debugging code. Want an image per manipulation step in Debug mode if enabled.
// TODO: Magic numbers to variables (Later controlled in class?).
// Find the board in an image and crop/scale/rectify so the image is of a planar board.
WarpResult warpToBoard(const cv::Mat& image, DebugVisualizer* debugger) {
	if (debugger) {
		debugger->beginStage("Warp To Board");
		debugger->add("Input", image);
	}

	if (image.empty()) {
		std::cerr << "Failed to load image\n";
		return {};
	}

	// 1. Preprocess image
	cv::Mat prepared;
	cv::cvtColor(image, prepared, cv::COLOR_BGR2GRAY); // Grayscale
	if (debugger)
		debugger->add("Grayscale", prepared);

	cv::GaussianBlur(prepared, prepared, cv::Size(7, 7), 1.5); // Gaussian Blur to reduce noise
	if (debugger)
		debugger->add("Gaussian Blur", prepared);

	cv::Canny(prepared, prepared, 50, 150); // Canny Edge Detection
	if (debugger)
		debugger->add("Canny Edge", prepared);

	// 2. Larger kernel merges thin internal lines
	cv::Mat closed;
	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(15, 15) // try 11–21 depending on resolution
	);
	cv::morphologyEx(prepared, closed, cv::MORPH_CLOSE, kernel);
	if (debugger)
		debugger->add("Morphology Close", closed);

	// 3. Find Contours
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	if (contours.empty()) {
		std::cerr << "No contours found\n";
		return {};
	}
	if (debugger) {
		auto drawnContours = image.clone();
		cv::drawContours(drawnContours, contours, -1, cv::Scalar(255, 0, 0), 2);
		debugger->add("Contour Finder", drawnContours);
	}

	// 4. Find largest contour
	double maxArea = 0.0;
	int bestIdx    = -1;

	for (int i = 0; i < (int)contours.size(); ++i) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) {
			maxArea = area;
			bestIdx = i;
		}
	}
	std::cout << "Largest contour idx: " << bestIdx << " area: " << maxArea << "\n";
	if (bestIdx < 0) {
		std::cerr << "No valid contour\n";
		return {};
	}
	const auto dominantContour = contours[bestIdx];
	if (debugger) {
		cv::Mat drawnContour = image.clone();
		cv::drawContours(drawnContour, contours, bestIdx, cv::Scalar(255, 0, 0), 3);
		debugger->add("Contour Largest", drawnContour);
	}

	// 5. Contour to a polygon
	std::vector<cv::Point> contourPolygon;
	double eps = 0.02 * cv::arcLength(dominantContour, true);
	cv::approxPolyDP(dominantContour, contourPolygon, eps, true);

	if (contourPolygon.size() != 4u) {
		// Rectangular Go Board requires 4 corners
		assert(false); // TODO: Not implemented but will happen occasionally.

		// TODO: Fallback to minAreaRect?
		/*
		cv::RotatedRect rr = cv::minAreaRect(dominantContour);
		cv::Point2f rrPts[4];
		rr.points(rrPts);
		std::vector<cv::Point2f> src(rrPts, rrPts + 4);
		src = orderCorners(std::vector<cv::Point>(convert rrPts to Points));
		*/
	}

	// 6. Do Warping (Normalise)
	const auto src               = orderCorners(contourPolygon);
	const int outSize            = 1000;
	std::vector<cv::Point2f> dst = {{0.f, 0.f}, {(float)outSize - 1.f, 0.f}, {(float)outSize - 1.f, (float)outSize - 1.f}, {0.f, (float)outSize - 1.f}};

	cv::Mat H = cv::getPerspectiveTransform(src, dst);

	cv::Mat warped;
	cv::warpPerspective(image, warped, H, cv::Size(outSize, outSize));
	if (debugger) {
		debugger->add("Warped", warped);
		debugger->endStage();
	}

	return {warped, H};
}

} // namespace go::camera