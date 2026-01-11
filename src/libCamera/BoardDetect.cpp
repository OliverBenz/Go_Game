#include "BoardDetect.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {

constexpr float kAngleToleranceDeg = 12.0f;
constexpr float kClusterEpsilon    = 8.0f;
constexpr float kCornerScaleStep   = 0.03f;

std::array<cv::Point2f, 4> orderCorners(const std::array<cv::Point2f, 4>& corners) {
	// Order: top-left, top-right, bottom-right, bottom-left.
	std::array<cv::Point2f, 4> ordered{};
	std::vector<cv::Point2f> pts(corners.begin(), corners.end());
	std::sort(pts.begin(), pts.end(), [](const cv::Point2f& a, const cv::Point2f& b) { return a.y < b.y; });

	const auto topLeft  = pts[0].x < pts[1].x ? pts[0] : pts[1];
	const auto topRight = pts[0].x < pts[1].x ? pts[1] : pts[0];
	const auto botLeft  = pts[2].x < pts[3].x ? pts[2] : pts[3];
	const auto botRight = pts[2].x < pts[3].x ? pts[3] : pts[2];

	ordered[0] = topLeft;
	ordered[1] = topRight;
	ordered[2] = botRight;
	ordered[3] = botLeft;
	return ordered;
}

std::array<cv::Point2f, 4> scaleCorners(const std::array<cv::Point2f, 4>& corners, float scale) {
	cv::Point2f center(0.0f, 0.0f);
	for (const auto& c : corners) {
		center += c;
	}
	center *= 0.25f;

	std::array<cv::Point2f, 4> scaled{};
	for (std::size_t i = 0; i < corners.size(); ++i) {
		scaled[i] = center + (corners[i] - center) * scale;
	}
	return scaled;
}

std::vector<float> clusterPositions(std::vector<float> positions) {
	std::vector<float> clusters;
	if (positions.empty()) {
		return clusters;
	}

	std::sort(positions.begin(), positions.end());
	float sum = positions[0];
	int count = 1;

	for (std::size_t i = 1; i < positions.size(); ++i) {
		if (std::abs(positions[i] - positions[i - 1]) <= kClusterEpsilon) {
			sum += positions[i];
			++count;
		} else {
			clusters.push_back(sum / static_cast<float>(count));
			sum   = positions[i];
			count = 1;
		}
	}
	clusters.push_back(sum / static_cast<float>(count));
	return clusters;
}

float meanSpacing(const std::vector<float>& values) {
	if (values.size() < 2) {
		return 0.0f;
	}
	float sum = 0.0f;
	for (std::size_t i = 1; i < values.size(); ++i) {
		sum += values[i] - values[i - 1];
	}
	return sum / static_cast<float>(values.size() - 1);
}

float spacingStdDev(const std::vector<float>& values, float mean) {
	if (values.size() < 2) {
		return 0.0f;
	}
	float sum = 0.0f;
	for (std::size_t i = 1; i < values.size(); ++i) {
		const float d = (values[i] - values[i - 1]) - mean;
		sum += d * d;
	}
	return std::sqrt(sum / static_cast<float>(values.size() - 1));
}

float polygonArea(const std::array<cv::Point2f, 4>& corners) {
	float area = 0.0f;
	for (std::size_t i = 0; i < corners.size(); ++i) {
		const auto& a = corners[i];
		const auto& b = corners[(i + 1) % corners.size()];
		area += (a.x * b.y) - (b.x * a.y);
	}
	return std::abs(area) * 0.5f;
}

} // namespace

BoardDetect::BoardDetect(std::string debugDir) : m_debugDir(std::move(debugDir)) {
}

unsigned BoardDetect::boardSize() const {
	return m_boardSize;
}

void BoardDetect::saveDebugImage(int step, const std::string& name, const cv::Mat& img) const {
	std::error_code ec;
	std::filesystem::create_directories(m_debugDir, ec);
	const auto filename = std::to_string(step) + "_" + name + ".png";
	const auto path     = std::filesystem::path(m_debugDir) / filename;
	if (!cv::imwrite(path.string(), img)) {
		std::cerr << "Failed to write debug image: " << path << "\n";
	}
}

std::vector<std::pair<unsigned, bool>> BoardDetect::process(const cv::Mat& img) {
	if (img.empty()) {
		return {};
	}

	saveDebugImage(0, "original", img);

	cv::Mat mask = buildBoardMask(img);
	saveDebugImage(1, "boardMask", mask);

	std::array<cv::Point2f, 4> corners{};
	cv::Mat contourDebug;
	if (!findBoardCorners(mask, corners, contourDebug) || !validateBoardCorners(corners, img.size())) {
		saveDebugImage(2, "boardContour", contourDebug.empty() ? img : contourDebug);
		return {};
	}
	saveDebugImage(2, "boardContour", contourDebug);

	cv::Mat warpDebug;
	const int warpSize = 800;
	cv::Mat warped;

	std::vector<float> xLines;
	std::vector<float> yLines;
	cv::Mat gridDebug;

	// Try scaled corner warps until grid detection looks plausible.
	bool warpOk = false;
	for (int attempt = 0; attempt < 4; ++attempt) {
		const float scale = 1.0f - static_cast<float>(attempt) * kCornerScaleStep;
		warped = warpBoard(img, corners, warpSize, scale, warpDebug);
		saveDebugImage(3, "warped_try" + std::to_string(attempt), warpDebug.empty() ? img : warpDebug);

		xLines.clear();
		yLines.clear();
		gridDebug.release();

		if (!detectGridLines(warped, xLines, yLines, gridDebug)) {
			saveDebugImage(4, "gridLines_try" + std::to_string(attempt), gridDebug.empty() ? warped : gridDebug);
			continue;
		}
		saveDebugImage(4, "gridLines_try" + std::to_string(attempt), gridDebug);

		if (validateGridLines(xLines, yLines)) {
			warpOk = true;
			break;
		}
	}

	if (!warpOk) {
		saveDebugImage(3, "warped", warped.empty() ? img : warped);
		saveDebugImage(4, "gridLines", gridDebug.empty() ? warped : gridDebug);
		return {};
	}

	saveDebugImage(3, "warped", warped);
	saveDebugImage(4, "gridLines", gridDebug);
	m_boardSize = estimateBoardSize(xLines.size(), yLines.size());

	float cellSize = 0.0f;
	if (xLines.size() >= 2 && yLines.size() >= 2) {
		cellSize = std::min(meanSpacing(xLines), meanSpacing(yLines));
	} else {
		cellSize = static_cast<float>(warpSize - 1) / static_cast<float>(m_boardSize - 1);
	}

	cv::Mat stonesDebug;
	auto stones = detectStones(warped, cellSize, stonesDebug);
	saveDebugImage(5, "stones", stonesDebug.empty() ? warped : stonesDebug);
	if (!validateStoneDetections(stones.size())) {
		return {};
	}

	std::vector<bool> colors;
	colors.reserve(stones.size());
	cv::Mat gray;
	cv::cvtColor(warped, gray, cv::COLOR_BGR2GRAY);
	for (const auto& stone: stones) {
		bool isBlack = false;
		if (classifyStone(gray, stone, isBlack)) {
			colors.push_back(isBlack);
		} else {
			colors.push_back(false);
		}
	}

	return mapStonePositions(stones, colors, warped.size(), xLines, yLines);
}

cv::Mat BoardDetect::buildBoardMask(const cv::Mat& img) const {
	cv::Mat blurred;
	cv::GaussianBlur(img, blurred, cv::Size(5, 5), 0);

	cv::Mat hsv;
	cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);

	// Sample center region to adapt to lighting and board tint.
	const int w  = hsv.cols;
	const int h  = hsv.rows;
	const int cx = w / 2;
	const int cy = h / 2;
	const int sw = std::min(std::max(10, w / 10), w);
	const int sh = std::min(std::max(10, h / 10), h);
	const int x0 = std::max(0, cx - sw / 2);
	const int y0 = std::max(0, cy - sh / 2);
	const int rw = std::min(sw, w - x0);
	const int rh = std::min(sh, h - y0);
	cv::Rect roi(x0, y0, rw, rh);
	const cv::Scalar meanHsv = cv::mean(hsv(roi));

	const int hTol   = 10;
	const int sTol   = 60;
	const int vTol   = 60;
	const auto clamp = [](int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); };

	const int hMin = clamp(static_cast<int>(meanHsv[0]) - hTol, 0, 179);
	const int hMax = clamp(static_cast<int>(meanHsv[0]) + hTol, 0, 179);
	const int sMin = clamp(static_cast<int>(meanHsv[1]) - sTol, 0, 255);
	const int sMax = clamp(static_cast<int>(meanHsv[1]) + sTol, 0, 255);
	const int vMin = clamp(static_cast<int>(meanHsv[2]) - vTol, 0, 255);
	const int vMax = clamp(static_cast<int>(meanHsv[2]) + vTol, 0, 255);

	cv::Mat mask;
	cv::inRange(hsv, cv::Scalar(hMin, sMin, vMin), cv::Scalar(hMax, sMax, vMax), mask);

	// NOTE: Closing helps connect the board region when edges are broken by glare.
	cv::Mat closed;
	cv::morphologyEx(mask, closed, cv::MORPH_CLOSE, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9)));
	return closed;
}

bool BoardDetect::findBoardCorners(const cv::Mat& mask, std::array<cv::Point2f, 4>& corners, cv::Mat& debug) const {
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (contours.empty()) {
		return false;
	}

	// Pick the largest contour as the board candidate.
	int maxIdx     = -1;
	double maxArea = 0.0;
	for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
		double area = cv::contourArea(contours[i]);
		if (area > maxArea) {
			maxArea = area;
			maxIdx  = i;
		}
	}

	const auto& contour = contours[maxIdx];
	std::vector<cv::Point> hull;
	cv::convexHull(contour, hull);

	std::vector<cv::Point> approx;
	cv::approxPolyDP(hull, approx, 0.02 * cv::arcLength(hull, true), true);

	std::array<cv::Point2f, 4> rawCorners{};
	if (approx.size() == 4) {
		for (int i = 0; i < 4; ++i) {
			rawCorners[i] = approx[i];
		}
	} else {
		// Fallback: use a minimum-area rectangle when we can't get four corners cleanly.
		const cv::RotatedRect rect = cv::minAreaRect(hull);
		rect.points(rawCorners.data());
	}

	corners = orderCorners(rawCorners);

	debug = cv::Mat::zeros(mask.size(), CV_8UC3);
	cv::cvtColor(mask, debug, cv::COLOR_GRAY2BGR);
	cv::drawContours(debug, contours, maxIdx, cv::Scalar(0, 255, 0), 2);
	for (const auto& c: corners) {
		cv::circle(debug, c, 6, cv::Scalar(0, 0, 255), -1);
	}

	return true;
}

bool BoardDetect::validateBoardCorners(const std::array<cv::Point2f, 4>& corners, const cv::Size& imgSize) const {
	const float area    = polygonArea(corners);
	const float imgArea = static_cast<float>(imgSize.width * imgSize.height);
	if (area < imgArea * 0.1f) {
		return false;
	}

	float minX = corners[0].x;
	float maxX = corners[0].x;
	float minY = corners[0].y;
	float maxY = corners[0].y;
	for (const auto& c : corners) {
		minX = std::min(minX, c.x);
		maxX = std::max(maxX, c.x);
		minY = std::min(minY, c.y);
		maxY = std::max(maxY, c.y);
	}

	const float width  = maxX - minX;
	const float height = maxY - minY;
	if (width <= 0.0f || height <= 0.0f) {
		return false;
	}

	const float aspect = width / height;
	return aspect > 0.6f && aspect < 1.4f;
}

cv::Mat BoardDetect::warpBoard(const cv::Mat& img,
                               const std::array<cv::Point2f, 4>& corners,
                               int outSize,
                               float scale,
                               cv::Mat& debug) const {
	const auto scaled                       = scaleCorners(corners, scale);
	const auto ordered                      = orderCorners(scaled);
	const std::array<cv::Point2f, 4> target = {
	        cv::Point2f(0.0f, 0.0f),
	        cv::Point2f(static_cast<float>(outSize - 1), 0.0f),
	        cv::Point2f(static_cast<float>(outSize - 1), static_cast<float>(outSize - 1)),
	        cv::Point2f(0.0f, static_cast<float>(outSize - 1)),
	};

	cv::Mat M = cv::getPerspectiveTransform(ordered.data(), target.data());
	cv::Mat warped;
	cv::warpPerspective(img, warped, M, cv::Size(outSize, outSize));

	debug = warped.clone();
	return warped;
}

bool BoardDetect::detectGridLines(const cv::Mat& warped, std::vector<float>& xLines, std::vector<float>& yLines, cv::Mat& debug) const {
	if (warped.empty()) {
		return false;
	}

	cv::Mat gray;
	cv::cvtColor(warped, gray, cv::COLOR_BGR2GRAY);
	cv::Mat edges;
	cv::Canny(gray, edges, 60, 180);

	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines, 1, CV_PI / 180.0, 100, 60, 10);

	std::vector<float> vertical;
	std::vector<float> horizontal;
	for (const auto& line: lines) {
		const float dx    = static_cast<float>(line[2] - line[0]);
		const float dy    = static_cast<float>(line[3] - line[1]);
		const float angle = std::atan2(dy, dx) * 180.0f / static_cast<float>(CV_PI);

		if (std::abs(angle) < kAngleToleranceDeg || std::abs(std::abs(angle) - 180.0f) < kAngleToleranceDeg) {
			horizontal.push_back((line[1] + line[3]) * 0.5f);
		} else if (std::abs(std::abs(angle) - 90.0f) < kAngleToleranceDeg) {
			vertical.push_back((line[0] + line[2]) * 0.5f);
		}
	}

	xLines = clusterPositions(vertical);
	yLines = clusterPositions(horizontal);

	debug = warped.clone();
	for (const auto& x: xLines) {
		cv::line(debug, cv::Point(static_cast<int>(x), 0), cv::Point(static_cast<int>(x), debug.rows - 1), cv::Scalar(255, 0, 0), 1);
	}
	for (const auto& y: yLines) {
		cv::line(debug, cv::Point(0, static_cast<int>(y)), cv::Point(debug.cols - 1, static_cast<int>(y)), cv::Scalar(0, 255, 0), 1);
	}

	return !xLines.empty() && !yLines.empty();
}

bool BoardDetect::validateGridLines(const std::vector<float>& xLines, const std::vector<float>& yLines) const {
	if (xLines.size() < 5 || yLines.size() < 5) {
		return false;
	}

	const std::array<unsigned, 3> candidates = {9, 13, 19};
	auto matchesCandidate = [&](std::size_t count) {
		for (unsigned candidate : candidates) {
			const std::size_t diff = count > candidate ? count - candidate : candidate - count;
			if (diff <= 2) {
				return true;
			}
		}
		return false;
	};

	if (!matchesCandidate(xLines.size()) || !matchesCandidate(yLines.size())) {
		return false;
	}

	const float xMean = meanSpacing(xLines);
	const float yMean = meanSpacing(yLines);
	if (xMean <= 1.0f || yMean <= 1.0f) {
		return false;
	}

	const float xStd = spacingStdDev(xLines, xMean);
	const float yStd = spacingStdDev(yLines, yMean);

	return (xStd / xMean) < 0.35f && (yStd / yMean) < 0.35f;
}

unsigned BoardDetect::estimateBoardSize(std::size_t xCount, std::size_t yCount) const {
	const std::size_t count                  = std::min(xCount, yCount);
	const std::array<unsigned, 3> candidates = {9, 13, 19};

	unsigned best      = 19;
	std::size_t bestDx = std::numeric_limits<std::size_t>::max();
	for (unsigned candidate: candidates) {
		const std::size_t dx = count > candidate ? count - candidate : candidate - count;
		if (dx < bestDx) {
			bestDx = dx;
			best   = candidate;
		}
	}

	// NOTE: If we cannot detect enough lines, fall back to 19.
	if (count < 5) {
		return 19;
	}
	return best;
}

std::vector<cv::Vec3f> BoardDetect::detectStones(const cv::Mat& warped, float cellSize, cv::Mat& debug) const {
	cv::Mat gray;
	cv::cvtColor(warped, gray, cv::COLOR_BGR2GRAY);
	cv::medianBlur(gray, gray, 5);

	const float minDist   = std::max(10.0f, cellSize * 0.7f);
	const float minRadius = std::max(5.0f, cellSize * 0.35f);
	const float maxRadius = std::max(minRadius + 2.0f, cellSize * 0.55f);

	std::vector<cv::Vec3f> circles;
	cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1.0, minDist, 120, 18, static_cast<int>(minRadius), static_cast<int>(maxRadius));

	debug = warped.clone();
	for (const auto& c: circles) {
		cv::Point center(cvRound(c[0]), cvRound(c[1]));
		int radius = cvRound(c[2]);
		cv::circle(debug, center, radius, cv::Scalar(0, 255, 0), 2);
		cv::circle(debug, center, 2, cv::Scalar(0, 0, 255), 3);
	}

	return circles;
}

bool BoardDetect::validateStoneDetections(std::size_t count) const {
	const std::size_t maxStones = static_cast<std::size_t>(m_boardSize) * static_cast<std::size_t>(m_boardSize);
	return count > 0 && count <= maxStones;
}

bool BoardDetect::classifyStone(const cv::Mat& gray, const cv::Vec3f& stone, bool& isBlack) const {
	const cv::Point center(cvRound(stone[0]), cvRound(stone[1]));
	const int radius = cvRound(stone[2]);

	cv::Mat stoneMask(gray.size(), CV_8UC1, cv::Scalar(0));
	cv::circle(stoneMask, center, radius - 2, cv::Scalar(255), -1);
	const cv::Scalar stoneMean = cv::mean(gray, stoneMask);

	cv::Mat ringMask(gray.size(), CV_8UC1, cv::Scalar(0));
	cv::circle(ringMask, center, radius + 6, cv::Scalar(255), 2);
	const cv::Scalar ringMean = cv::mean(gray, ringMask);

	// NOTE: Compare against local background to handle global lighting changes.
	const double value = stoneMean[0];
	const double back  = ringMean[0];
	if (back <= 0.0) {
		return false;
	}
	isBlack = value < (back * 0.7);
	return true;
}

std::vector<std::pair<unsigned, bool>> BoardDetect::mapStonePositions(const std::vector<cv::Vec3f>& stones, const std::vector<bool>& colors,
                                                                      const cv::Size& boardSizePx, const std::vector<float>& xLines,
                                                                      const std::vector<float>& yLines) const {
	std::vector<float> xs = xLines;
	std::vector<float> ys = yLines;

	if (xs.empty() || ys.empty()) {
		// NOTE: Fallback to evenly spaced grid if line detection failed.
		const float step = static_cast<float>(boardSizePx.width - 1) / static_cast<float>(m_boardSize - 1);
		xs.resize(m_boardSize);
		ys.resize(m_boardSize);
		for (unsigned i = 0; i < m_boardSize; ++i) {
			xs[i] = i * step;
			ys[i] = i * step;
		}
	}

	std::vector<std::pair<unsigned, bool>> result;
	result.reserve(stones.size());
	std::vector<bool> occupied(xs.size() * ys.size(), false);

	for (std::size_t i = 0; i < stones.size(); ++i) {
		const auto& stone = stones[i];
		const float x     = stone[0];
		const float y     = stone[1];

		auto nearestIndex = [](const std::vector<float>& values, float target) {
			std::size_t best = 0;
			float bestDist   = std::numeric_limits<float>::max();
			for (std::size_t idx = 0; idx < values.size(); ++idx) {
				const float d = std::abs(values[idx] - target);
				if (d < bestDist) {
					bestDist = d;
					best     = idx;
				}
			}
			return best;
		};

		const std::size_t col = nearestIndex(xs, x);
		const std::size_t row = nearestIndex(ys, y);
		const unsigned index  = static_cast<unsigned>(row * xs.size() + col);
		const bool isBlack    = i < colors.size() ? colors[i] : false;
		if (index < occupied.size() && !occupied[index]) {
			occupied[index] = true;
			result.emplace_back(index, isBlack);
		}
	}

	return result;
}
