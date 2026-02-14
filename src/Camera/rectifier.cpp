#include "camera/rectifier.hpp"

#include "statistics.hpp"
#include "gridFinder.hpp"

namespace go::camera {
namespace debugging {

//! Draw lines onto an image.
cv::Mat drawLines(const cv::Mat& image, const std::vector<double>& vertical, const std::vector<double>& horizontal) {
	cv::Mat drawnLines = image.clone();  // Use warped image ideally

    for (double x : vertical) {
        int xi = static_cast<int>(std::lround(x));
        cv::line(drawnLines,
                 cv::Point(xi, 0),
                 cv::Point(xi, drawnLines.rows - 1),
                 cv::Scalar(255, 0, 0),
                 3);
    }

    // Draw clustered horizontal grid lines (thick yellow)
    for (double y : horizontal) {
        int yi = static_cast<int>(std::lround(y));
        cv::line(drawnLines,
                 cv::Point(0, yi),
                 cv::Point(drawnLines.cols - 1, yi),
                 cv::Scalar(100, 0, 150),
                 3);
    }
	return drawnLines;
	
}

}


namespace internal {
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
	cv::cvtColor(image, prepared, cv::COLOR_BGR2GRAY);         // Grayscale
	if (debugger) debugger->add("Grayscale", prepared);

	cv::GaussianBlur(prepared, prepared, cv::Size(7, 7), 1.5); // Gaussian Blur to reduce noise
	if (debugger) debugger->add("Gaussian Blur", prepared);

	cv::Canny(prepared, prepared, 50, 150);                    // Canny Edge Detection
	if (debugger) debugger->add("Canny Edge", prepared);

	// 2. Larger kernel merges thin internal lines
	cv::Mat closed;
	cv::Mat kernel = cv::getStructuringElement(
		cv::MORPH_RECT,
		cv::Size(15, 15)   // try 11–21 depending on resolution
	);
	cv::morphologyEx(prepared, closed, cv::MORPH_CLOSE, kernel);
	if (debugger) debugger->add("Morphology Close", closed);

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
	if (debugger) {
		cv::Mat drawnContour = image.clone();
		cv::drawContours(drawnContour, contours, bestIdx, cv::Scalar(255, 0, 0), 3);
		debugger->add("Contour Largest", drawnContour);
	}

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
	if (debugger) {
		debugger->add("Warped", warped);
		debugger->endStage();
	}

	return {warped, H};
}

}


struct Line1D {
	double pos;     // x for vertical, y for horizontal
	double weight;  // e.g. segment length
};

static std::vector<double> clusterWeighted1D(std::vector<Line1D> values, double eps) {
    if (values.empty()) {
		return {};
	};

	// Sort lines by position
    std::sort(values.begin(), values.end(),
              [](const Line1D& a, const Line1D& b){ return a.pos < b.pos; });

    std::vector<double> centers;
    double wSum = values[0].weight;
    double pSum = values[0].pos * values[0].weight;
    for (size_t i = 1; i < values.size(); ++i) {
        if (std::abs(values[i].pos - values[i-1].pos) <= eps) {
            wSum += values[i].weight;
            pSum += values[i].pos * values[i].weight;
        } else {
            centers.push_back(pSum / wSum);
            wSum = values[i].weight;
            pSum = values[i].pos * values[i].weight;
        }
    }
    centers.push_back(pSum / wSum);

    return centers;
}

static double computeMedianSpacing(const std::vector<double>& grid)
{
    assert(grid.size() >= 2);

    std::vector<double> diffs;
    diffs.reserve(grid.size() - 1);
    for (size_t i = 1; i < grid.size(); ++i)
        diffs.push_back(grid[i] - grid[i - 1]);

    return median(diffs);
}

//! Transform an image that contains a Go Board such that the final image is a top-down projection of the board.
//! \note The border of the image is the outermost grid line + tolerance for the edge stones.
cv::Mat rectifyImage(const cv::Mat& image, DebugVisualizer* debugger) {
	// 0. Input: Roughly warped image
	auto [warped, warpHomography] = internal::warpToBoard(image, debugger); //!< Warped for better grid detection.
	
	if (debugger) {
		debugger->beginStage("Rectify Image");
		debugger->add("Input", warped);
	}

	// TODO: Properly rotate at some point. Roughly rotate in warpToBoard() and fine rotate here.


	// 1. Preprocess again
	cv::Mat gray, blur, edges;
	cv::cvtColor(warped, gray, cv::COLOR_BGR2GRAY);           // Greyscale
	if (debugger) debugger->add("Grayscale", gray);

	cv::GaussianBlur(gray, blur, cv::Size(9,9), 1.5);         // Blur to reduce noise
	if (debugger) debugger->add("Gaussian Blur", blur);

	cv::Canny(blur, edges, 50, 120);                          // Edge detection
	if (debugger) debugger->add("Canny Edge", edges);

	cv::dilate(edges, edges, cv::Mat(), cv::Point(-1,-1), 1); // Cleanup detected edges
	if (debugger) debugger->add("Dilate Canny", edges);

	// 2. Find horizontal and vertical line candidates (merge close together lines but there are not necessarily our grid yet)
	// Find line segments
	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines,
		1,                // rho resolution
		CV_PI / 180,      // theta resolution
		80,               // threshold (votes)
		100,              // minLineLength
		20                // maxLineGap
	);

	// Separate horizontal / vertical lines
	std::vector<cv::Vec4i> vertical;
	std::vector<cv::Vec4i> horizontal;

	for (const auto& l : lines) {
		float dx = l[2] - l[0];
		float dy = l[3] - l[1];

		float angle = std::atan2(dy, dx) * 180.0f / CV_PI;

		// Normalize angle to [-90, 90]
		while (angle < -90) angle += 180;
		while (angle >  90) angle -= 180;

		if (std::abs(angle) < 15) {
			horizontal.push_back(l);
		}
		else if (std::abs(angle) > 75) {
			vertical.push_back(l);
		}
	}
	std::cout << "Vertical lines: " << vertical.size() << "\n Horizontal lines: " << horizontal.size() << "\n";

	// Group together lines (one grid line has finite thickness -> detected as many lines)
	std::vector<Line1D> v1d, h1d;
	auto segLen = [](const cv::Vec4i& l) {
		double dx = l[2] - l[0];
		double dy = l[3] - l[1];
		return std::sqrt(dx*dx + dy*dy);
	};
	for (const auto& l : vertical) {
		v1d.push_back({0.5 * (l[0] + l[2]), segLen(l)});
	}
	for (const auto& l : horizontal) {
		h1d.push_back({0.5 * (l[1] + l[3]), segLen(l)});
	}

	// Merge lines close together
	double mergeEps = 15.0; //!< In pixels
	auto vGrid = clusterWeighted1D(v1d, mergeEps);
	auto hGrid = clusterWeighted1D(h1d, mergeEps);

	const auto Nv = vGrid.size();
	const auto Nh = hGrid.size();

	std::cout << "Unique vertical candidates: " << Nv << "\n";
	std::cout << "Unique horizontal candidates: " << Nh << "\n";
	if (debugger) {
		debugger->add("Grid Candidates", debugging::drawLines(warped, vGrid, hGrid));
	}

	// 3. Grid candidates to proper grid.
	// Check if grid found. Else try with another algorithm.
	if (Nv == Nh && (Nv == 9 || Nv == 13 || Nv == 19)) {
		std::cout << "Board size determined directly: " << Nv << "\n";

#ifndef NDEBUG
	// Debug: Verify if the grid is found with a second algorithm.
	std::vector<double> vGridTest{}, hGridTest{};
	assert(findGrid(vGrid, hGrid, vGridTest, hGridTest));
	assert(vGridTest.size() == hGridTest.size());
	assert(vGridTest.size() == vGrid.size() && hGridTest.size() == hGrid.size());
#endif
	} else {
		std::cout << "Could not detect the board size trivially. Performing further steps.\n";
		
		std::vector<double> vGridAttempt{};
		std::vector<double> hGridAttempt{};
		if (!findGrid(vGrid, hGrid, vGridAttempt, hGridAttempt)) {
			std::cerr << "Could not detect a valid grid. Stopping!\n";
			return {};
		}

		vGrid = vGridAttempt;
		hGrid = hGridAttempt;
	}

	// Starting here, we assume grid found
	// 4. Warp image with stone buffer at edge (want to detect full stone at edge)
	double spacingX = computeMedianSpacing(vGrid);
	double spacingY = computeMedianSpacing(hGrid);
	double spacing = 0.5 * (spacingX + spacingY);  //!< Spacing between grid lines.

	double stoneBuffer = 0.5 * spacing; // NOTE: Could adjust 0.5 to account for imperfect placement.

	float xmin = static_cast<float>(vGrid.front() - stoneBuffer);
	float xmax = static_cast<float>(vGrid.back()  + stoneBuffer);
	float ymin = static_cast<float>(hGrid.front() - stoneBuffer);
	float ymax = static_cast<float>(hGrid.back()  + stoneBuffer);

	// Perform the warping on the original image (avoid getting black bars if the new image is larger than the warped(after first step) one)
	// Board coordinates in the warped image
	std::vector<cv::Point2f> srcWarped = {
		{xmin, ymin},
		{xmax, ymin},
		{xmax, ymax},
		{xmin, ymax}
	};

	// Invert the previous warping so we can apply our new warp on the original image.
	cv::Mat Hinv = warpHomography.inv();
	std::vector<cv::Point2f> srcOriginal;
	cv::perspectiveTransform(srcWarped, srcOriginal, Hinv);

	// Output range
	const int outSize = 1000;
	std::vector<cv::Point2f> dst = {
		{0.f, 0.f},
		{(float)outSize - 1.f, 0.f},
		{(float)outSize - 1.f, (float)outSize - 1.f},
		{0.f, (float)outSize - 1.f}
	};

	cv::Mat homographyFinal = cv::getPerspectiveTransform(srcOriginal, dst);
	cv::Mat refined;
	cv::warpPerspective(image, refined, homographyFinal, cv::Size(outSize, outSize));
	if (debugger) {
		debugger->add("Warp Image", refined);
		debugger->endStage();
	}
	
	// TODO: Rotate nicely horizontally

	std::cout << "\n\n";
	return refined;
}

}