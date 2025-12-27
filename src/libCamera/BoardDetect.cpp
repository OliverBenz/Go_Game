#include "BoardDetect.hpp"

BoardDetect::BoardDetect() {
	// Initial setup with manual values
	SetupBoardMask();
	SetupStoneMask();
}

std::vector<std::pair<unsigned, bool>> BoardDetect::process(const cv::Mat& img) {
	// auto res = detectBoardContour();
	// res = warpAndCutBoard(res);

	// auto stones = detectStones(res);
	// return calcStonePositionIndex(stones);
	return {};
}

void BoardDetect::SetupBoardMask(const cv::Mat& img) {
	// HSV Range will be properly set at
	static int hmin = 13, smin = 44, vmin = 187;
	static int hmax = 37, smax = 197, vmax = 255;

	cv::Mat hsv;
	cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
	cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), m_boardMask);

	// Histogram equalization or CLAHE on V channel improves detection under variable lighting.
}

cv::Mat BoardDetect::DetectBoardContour(const cv::Mat& img) {
	// Consider morphological operations (MORPH_CLOSE / MORPH_OPEN) to clean up the mask before finding contours.

	// If fails       -> SetupBoardMask and try again (maybe lighting changed)
	// If still fails -> Gracefully fail

	cv::Mat result;
	cv::bitwise_and(img, img, result, m_boardMask);
	return result;
}

cv::Mat BoardDetect::WarpAndCutBoard(const cv::Mat& img) {
	// Im assuming a quadrilateral. If the contour has extra noise, approxPolyDP may produce >4 points.
	//  -> If more than 4 points, consider convex hull or polygon simplification to reliably pick corners.
	return {};
}

bool BoardDetect::VerifyBoardWarp(const cv::Mat& img) {
	return true;
}

std::vector<std::pair<cv::Vec3f, bool>> BoardDetect::DetectStones(const cv::Mat& img) {
	// Consider masking only inside board area before stone detection. This reduces false positives.

	return {};

	// Robust way to distinguish black vs white stones:
	// Use V (value) channel in HSV or grayscale mean intensity.
	// Normalize by local background (to account for shadows).
	// Optional: adaptive threshold or simple learning-based method if lighting varies a lot.

	// Optional: color code stones by detection confidence.
}

bool BoardDetect::VerifyDetectStones() {
	return true;
}

std::vector<std::pair<unsigned, bool>> BoardDetect::calcStonePositionIndex(const std::vector<std::pair<cv::Vec3f>>& stones) {
	// Board warp may have small scaling errors (both physical warp and image warp)
	// Stones not perfectly centered in intersections
	return {};
}
