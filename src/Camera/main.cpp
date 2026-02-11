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

void analyseBoard(cv::Mat& image) {
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

// Find the board in an image and crop/scale/rectify so the image is of a planar board. 
cv::Mat rectifyImage(const cv::Mat& image) {
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
        return image;
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
		return image;
	}




	// Draw contour for testing
	cv::Mat contourVis = image.clone(); // BGR
	cv::drawContours(
		contourVis,
		contours,
		bestIdx,                          // -1 = draw all
		cv::Scalar(0, 255, 0),       // green
		3                            // thickness
	);
	return contourVis;
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
	cv::Mat image9 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_9.jpeg");
	cv::Mat image13 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_13.jpeg");
	cv::Mat image19 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_19.jpeg");
	
	auto rect9  = rectifyImage(image9);
	auto rect13 = rectifyImage(image13);
	auto rect19 = rectifyImage(image19);

	showImages(rect9, rect13, rect19);

	//analyseBoard(image);

	return 0;
}
