#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <opencv2/opencv.hpp>

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

void analyseBoard(const cv::Mat& image) {
	if (image.empty()) {
		std::cerr << "Failed to load image\n";
		return;
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

    auto sumCmp = [](const cv::Point2f& a, const cv::Point2f& b) {
        return (a.x + a.y) < (b.x + b.y);
    };
    auto diffCmp = [](const cv::Point2f& a, const cv::Point2f& b) {
        return (a.x - a.y) < (b.x - b.y);
    };

    ordered[0] = *std::min_element(pts.begin(), pts.end(), sumCmp);  // TL
    ordered[1] = *std::min_element(pts.begin(), pts.end(), diffCmp); // TR
    ordered[2] = *std::max_element(pts.begin(), pts.end(), sumCmp);  // BR
    ordered[3] = *std::max_element(pts.begin(), pts.end(), diffCmp); // BL

    return ordered;
}

// TODO: Add some debugging code. Want an image per manipulation step in Debug mode if enabled.
// TODO: Magic numbers to variables (Later controlled in class?).
// Find the board in an image and crop/scale/rectify so the image is of a planar board. 
cv::Mat warpToBoard(const cv::Mat& image) {
	if (image.empty()) {
		std::cerr << "Failed to load image\n";
		return {};
	}

	// 1. Preprocess image
	cv::Mat prepared;
	cv::cvtColor(image, prepared, cv::COLOR_BGR2GRAY);         // Grayscale
	cv::GaussianBlur(prepared, prepared, cv::Size(7, 7), 1.5); // Gaussian Blur to reduce noise
	cv::Canny(prepared, prepared, 50, 150);                    // Canny Edge Detection

	// 2. Larger kernel merges thin internal lines
	cv::Mat closed;
	cv::Mat kernel = cv::getStructuringElement(
		cv::MORPH_RECT,
		cv::Size(15, 15)   // try 11–21 depending on resolution
	);
	cv::morphologyEx(prepared, closed, cv::MORPH_CLOSE, kernel);

	// 3. Find Contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        std::cerr << "No contours found\n";
        return {};
    }

	// 4. Find largest contour
	double maxArea = 0.0;
	int bestIdx = -1;

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

	// 5. Contour to a polygon
	std::vector<cv::Point> contourPolygon;
	double eps = 0.02 * cv::arcLength(dominantContour, true);
	cv::approxPolyDP(dominantContour, contourPolygon, eps, true);

	if(contourPolygon.size() != 4u) {
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
	const auto src = orderCorners(contourPolygon);
	const int outSize = 1000;
	std::vector<cv::Point2f> dst = {
		{0.f, 0.f},
		{(float)outSize - 1.f, 0.f},
		{(float)outSize - 1.f, (float)outSize - 1.f},
		{0.f, (float)outSize - 1.f}
	};

	cv::Mat H = cv::getPerspectiveTransform(src, dst);

	cv::Mat warped;
	cv::warpPerspective(image, warped, H, cv::Size(outSize, outSize));

	return warped;
}

//! Transform an image that contains a Go Board such that the final image is a top-down projection of the board.
//! \note The border of the image is the outermost grid line + tolerance for the edge stones.
cv::Mat rectifyImage(const cv::Mat& image) {
	cv::Mat rectified = warpToBoard(image); //!< Warped for better grid detection.
	
	// TODO: warp the image such that image border=outermost grid lines (+ tolerance for stones on edge).
	return rectified;
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

int main() {
	// Load Image
	cv::Mat image9  = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_9.jpeg");
	cv::Mat image13 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_13.jpeg");
	cv::Mat image19 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_19.jpeg");
	
	auto rect9  = rectifyImage(image9);
	auto rect13 = rectifyImage(image13);
	auto rect19 = rectifyImage(image19);

	showImages(rect9, rect13, rect19);

	//analyseBoard(image);

	return 0;
}
