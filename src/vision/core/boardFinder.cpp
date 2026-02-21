#include "camera/boardFinder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

namespace tengen::vision::core {

namespace {

//! Normalized output size used by rough board warp.
static constexpr int WARP_OUT_SIZE = 1000;
//! Number of largest contours considered for board-candidate scoring.
static constexpr int MAX_CANDIDATE_CONTOURS = 24;
//! Candidate quad must cover at least this image-area fraction.
static constexpr double MIN_CONTOUR_AREA_FRAC = 0.08;

//! Enable verbose board-candidate diagnostics via environment variable.
static bool boardDebugEnabled() {
	const char* env = std::getenv("GO_BOARD_DEBUG");
	return env != nullptr && std::string_view(env) == "1";
}

//! Ensure odd kernel sizes for blur/morphology operations.
static int makeOddKernelSize(int value) {
	return (value % 2 == 0) ? value + 1 : value;
}

//! Preprocessing settings used to construct stable board-edge masks.
struct PreprocessSettings {
	int blurKernelSize{7};
	int closeKernelSize{15};
	double cannyLow{50.0};
	double cannyHigh{150.0};
};

//! Masks generated from different preprocessing paths.
struct CandidateMasks {
	cv::Mat edgeMask;
	cv::Mat brightMask;
	cv::Mat darkMask;
};

//! Select preprocessing settings from image size to stay robust across resolutions.
static PreprocessSettings choosePreprocessSettings(const cv::Size imageSize) {
	const int minDim = std::max(1, std::min(imageSize.width, imageSize.height));

	PreprocessSettings settings{};
	settings.blurKernelSize  = makeOddKernelSize(std::clamp(minDim / 150, 3, 9));
	settings.closeKernelSize = makeOddKernelSize(std::clamp(minDim / 68, 9, 25));
	settings.cannyLow        = std::clamp(0.05 * static_cast<double>(minDim), 45.0, 60.0);
	settings.cannyHigh       = std::clamp(0.15 * static_cast<double>(minDim), 135.0, 180.0);
	if (settings.cannyHigh <= settings.cannyLow + 1.0) {
		settings.cannyHigh = settings.cannyLow + 1.0;
	}

	return settings;
}

//! Convert image to grayscale independent of channel format.
static bool convertToGray(const cv::Mat& image, cv::Mat& outGray) {
	if (image.channels() == 1) {
		outGray = image.clone();
		return true;
	}
	if (image.channels() == 3) {
		cv::cvtColor(image, outGray, cv::COLOR_BGR2GRAY);
		return true;
	}
	if (image.channels() == 4) {
		cv::cvtColor(image, outGray, cv::COLOR_BGRA2GRAY);
		return true;
	}
	return false;
}

//! Build complementary binary masks used for contour extraction.
static CandidateMasks buildCandidateMasks(const cv::Mat& blurredGray, const PreprocessSettings& settings) {
	CandidateMasks masks{};

	// Edge-driven mask.
	cv::Mat edges;
	cv::Canny(blurredGray, edges, settings.cannyLow, settings.cannyHigh);
	const cv::Mat edgeKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(settings.closeKernelSize, settings.closeKernelSize));
	cv::morphologyEx(edges, masks.edgeMask, cv::MORPH_CLOSE, edgeKernel);

	// Intensity-driven masks (both polarities).
	cv::Mat otsuMask;
	cv::threshold(blurredGray, otsuMask, 0.0, 255.0, cv::THRESH_BINARY | cv::THRESH_OTSU);

	const int intensityKernelSize = makeOddKernelSize(settings.closeKernelSize + 4);
	const cv::Mat intensityKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(intensityKernelSize, intensityKernelSize));
	cv::morphologyEx(otsuMask, masks.brightMask, cv::MORPH_CLOSE, intensityKernel);

	cv::Mat inverted = cv::Scalar(255) - otsuMask;
	cv::morphologyEx(inverted, masks.darkMask, cv::MORPH_CLOSE, intensityKernel);

	return masks;
}

//! Order 4 corner points TL,TR,BR,BL.
static std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point2f>& quad) {
	CV_Assert(quad.size() == 4u);

	const auto idxMinSum =
	        static_cast<int>(std::min_element(quad.begin(), quad.end(),
	                                          [](const cv::Point2f& left, const cv::Point2f& right) { return (left.x + left.y) < (right.x + right.y); }) -
	                         quad.begin());
	const auto idxMinDiff =
	        static_cast<int>(std::min_element(quad.begin(), quad.end(),
	                                          [](const cv::Point2f& left, const cv::Point2f& right) { return (left.x - left.y) < (right.x - right.y); }) -
	                         quad.begin());
	const auto idxMaxSum =
	        static_cast<int>(std::max_element(quad.begin(), quad.end(),
	                                          [](const cv::Point2f& left, const cv::Point2f& right) { return (left.x + left.y) < (right.x + right.y); }) -
	                         quad.begin());
	const auto idxMaxDiff =
	        static_cast<int>(std::max_element(quad.begin(), quad.end(),
	                                          [](const cv::Point2f& left, const cv::Point2f& right) { return (left.x - left.y) < (right.x - right.y); }) -
	                         quad.begin());

	const bool unique = idxMinSum != idxMinDiff && idxMinSum != idxMaxSum && idxMinSum != idxMaxDiff && idxMinDiff != idxMaxSum && idxMinDiff != idxMaxDiff &&
	                    idxMaxSum != idxMaxDiff;
	if (unique) {
		return {
		        quad[static_cast<std::size_t>(idxMinSum)],
		        quad[static_cast<std::size_t>(idxMinDiff)],
		        quad[static_cast<std::size_t>(idxMaxSum)],
		        quad[static_cast<std::size_t>(idxMaxDiff)],
		};
	}

	// Fallback: sort by Y, then split top/bottom and sort by X.
	std::array<cv::Point2f, 4> sortedByY{};
	for (std::size_t i = 0u; i < 4u; ++i) {
		sortedByY[i] = quad[i];
	}
	std::sort(sortedByY.begin(), sortedByY.end(), [](const cv::Point2f& left, const cv::Point2f& right) {
		if (left.y == right.y) {
			return left.x < right.x;
		}
		return left.y < right.y;
	});

	std::array<cv::Point2f, 2> topRow{sortedByY[0], sortedByY[1]};
	std::array<cv::Point2f, 2> bottomRow{sortedByY[2], sortedByY[3]};
	std::sort(topRow.begin(), topRow.end(), [](const cv::Point2f& left, const cv::Point2f& right) { return left.x < right.x; });
	std::sort(bottomRow.begin(), bottomRow.end(), [](const cv::Point2f& left, const cv::Point2f& right) { return left.x < right.x; });

	return {topRow[0], topRow[1], bottomRow[1], bottomRow[0]};
}

//! Compute cosine of corner angle p0-p1-p2.
static double cornerCosine(const cv::Point2f& p0, const cv::Point2f& p1, const cv::Point2f& p2) {
	const cv::Point2f v1 = p0 - p1;
	const cv::Point2f v2 = p2 - p1;
	const double norm1   = cv::norm(v1);
	const double norm2   = cv::norm(v2);
	if (norm1 <= 1e-6 || norm2 <= 1e-6) {
		return 1.0;
	}
	return std::abs((v1.x * v2.x + v1.y * v2.y) / (norm1 * norm2));
}

//! Compute absolute cosine between two vectors (parallel/anti-parallel -> 1).
static double parallelCosine(const cv::Point2f& v0, const cv::Point2f& v1) {
	const double norm0 = cv::norm(v0);
	const double norm1 = cv::norm(v1);
	if (norm0 <= 1e-6 || norm1 <= 1e-6) {
		return 0.0;
	}
	return std::abs((v0.x * v1.x + v0.y * v1.y) / (norm0 * norm1));
}

//! Quad candidate extracted from one contour.
struct QuadCandidate {
	std::vector<cv::Point2f> quad;
	bool fromApprox{false};
	double contourArea{0.0};
};

//! Weighted 1D line center used for line-center clustering.
struct Line1D {
	double pos{0.0};
	double weight{0.0};
};

//! Grid evidence extracted from a warped board candidate.
struct GridEvidence {
	double score{0.0};
	int verticalCount{0};
	int horizontalCount{0};
};

//! Try multiple approximation epsilons until a convex 4-corner polygon is found.
static bool contourToApproxQuad(const std::vector<cv::Point>& contour, std::vector<cv::Point2f>& outQuad) {
	const double perimeter                 = cv::arcLength(contour, true);
	const std::array<double, 6> epsFactors = {0.012, 0.016, 0.020, 0.026, 0.032, 0.040};

	std::vector<cv::Point> polygon;
	for (double epsFactor: epsFactors) {
		cv::approxPolyDP(contour, polygon, epsFactor * perimeter, true);
		if (polygon.size() == 4u && cv::isContourConvex(polygon)) {
			outQuad.resize(4u);
			for (std::size_t i = 0u; i < 4u; ++i) {
				outQuad[i] = polygon[i];
			}
			outQuad = orderCorners(outQuad);
			return true;
		}
	}

	return false;
}

//! Convert contour to quad using polygon approximation, then minAreaRect fallback.
static QuadCandidate contourToQuad(const std::vector<cv::Point>& contour) {
	const double contourArea = std::abs(cv::contourArea(contour));

	std::vector<cv::Point2f> approxQuad;
	if (contourToApproxQuad(contour, approxQuad)) {
		return {std::move(approxQuad), true, contourArea};
	}

	cv::RotatedRect rect = cv::minAreaRect(contour);
	std::array<cv::Point2f, 4> points{};
	rect.points(points.data());
	std::vector<cv::Point2f> quad(points.begin(), points.end());
	return {orderCorners(quad), false, contourArea};
}

//! Cluster close weighted line centers into single representative centers.
static std::vector<double> clusterWeighted1D(std::vector<Line1D> values, double eps) {
	if (values.empty()) {
		return {};
	}

	std::sort(values.begin(), values.end(), [](const Line1D& left, const Line1D& right) { return left.pos < right.pos; });

	std::vector<double> centers;
	double weightSum   = values.front().weight;
	double weightedPos = values.front().pos * values.front().weight;
	for (std::size_t i = 1u; i < values.size(); ++i) {
		if (std::abs(values[i].pos - values[i - 1].pos) <= eps) {
			weightSum += values[i].weight;
			weightedPos += values[i].pos * values[i].weight;
		} else {
			centers.push_back(weightedPos / weightSum);
			weightSum   = values[i].weight;
			weightedPos = values[i].pos * values[i].weight;
		}
	}
	centers.push_back(weightedPos / weightSum);
	return centers;
}

//! Score how close a line-count is to legal Go board sizes.
static double boardLineCountScore(const int count) {
	const int d9   = std::abs(count - 9);
	const int d13  = std::abs(count - 13);
	const int d19  = std::abs(count - 19);
	const int best = std::min({d9, d13, d19});
	return std::clamp(1.0 - static_cast<double>(best) / 8.0, 0.0, 1.0);
}

//! Evaluate grid-line evidence for one board candidate using a fast line-count check on the warped candidate.
static GridEvidence evaluateGridEvidence(const cv::Mat& image, const std::vector<cv::Point2f>& quad) {
	GridEvidence evidence{};
	if (image.empty() || quad.size() != 4u) {
		return evidence;
	}

	const std::vector<cv::Point2f> dst = {
	        {0.f, 0.f},
	        {static_cast<float>(WARP_OUT_SIZE) - 1.f, 0.f},
	        {static_cast<float>(WARP_OUT_SIZE) - 1.f, static_cast<float>(WARP_OUT_SIZE) - 1.f},
	        {0.f, static_cast<float>(WARP_OUT_SIZE) - 1.f},
	};

	cv::Mat H = cv::getPerspectiveTransform(quad, dst);

	cv::Mat warped;
	cv::warpPerspective(image, warped, H, cv::Size(WARP_OUT_SIZE, WARP_OUT_SIZE));
	if (warped.empty()) {
		return evidence;
	}

	cv::Mat gray;
	if (!convertToGray(warped, gray)) {
		return evidence;
	}

	cv::Mat blur;
	cv::GaussianBlur(gray, blur, cv::Size(9, 9), 1.5);

	cv::Mat edges;
	cv::Canny(blur, edges, 50, 120);
	cv::dilate(edges, edges, cv::Mat(), cv::Point(-1, -1), 1);

	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines, 1.0, CV_PI / 180.0, 80, 100, 20);
	if (lines.empty()) {
		return evidence;
	}

	std::vector<Line1D> vertical;
	std::vector<Line1D> horizontal;
	vertical.reserve(lines.size());
	horizontal.reserve(lines.size());
	for (const auto& line: lines) {
		const double dx     = static_cast<double>(line[2] - line[0]);
		const double dy     = static_cast<double>(line[3] - line[1]);
		const double segLen = std::sqrt(dx * dx + dy * dy);
		double angle        = std::atan2(dy, dx) * 180.0 / CV_PI;
		while (angle < -90.0)
			angle += 180.0;
		while (angle > 90.0)
			angle -= 180.0;

		if (std::abs(angle) < 15.0) {
			horizontal.push_back({0.5 * static_cast<double>(line[1] + line[3]), segLen});
		} else if (std::abs(angle) > 75.0) {
			vertical.push_back({0.5 * static_cast<double>(line[0] + line[2]), segLen});
		}
	}

	const std::vector<double> vCenters = clusterWeighted1D(vertical, 15.0);
	const std::vector<double> hCenters = clusterWeighted1D(horizontal, 15.0);

	evidence.verticalCount   = static_cast<int>(vCenters.size());
	evidence.horizontalCount = static_cast<int>(hCenters.size());

	const double fitV           = boardLineCountScore(evidence.verticalCount);
	const double fitH           = boardLineCountScore(evidence.horizontalCount);
	const double pairFit        = std::sqrt(fitV * fitH);
	const double balance        = std::clamp(1.0 - std::abs(evidence.verticalCount - evidence.horizontalCount) / 12.0, 0.0, 1.0);
	const double segmentSupport = std::clamp((static_cast<double>(vertical.size()) + static_cast<double>(horizontal.size())) / 220.0, 0.0, 1.0);

	evidence.score = 2.0 * pairFit + 0.8 * balance + 0.4 * segmentSupport;
	return evidence;
}

//! Check whether a quad satisfies geometric constraints expected for a physical Go board.
static bool isPlausibleBoardQuad(const std::vector<cv::Point2f>& quad, const cv::Size imageSize) {
	if (quad.size() != 4u) {
		return false;
	}

	const double imageArea = static_cast<double>(imageSize.width) * static_cast<double>(imageSize.height);
	const double area      = std::abs(cv::contourArea(quad));
	if (area < MIN_CONTOUR_AREA_FRAC * imageArea) {
		return false;
	}
	if (area > 0.75 * imageArea) {
		return false;
	}

	const double top    = cv::norm(quad[1] - quad[0]);
	const double right  = cv::norm(quad[2] - quad[1]);
	const double bottom = cv::norm(quad[2] - quad[3]);
	const double left   = cv::norm(quad[3] - quad[0]);
	const double minLen = std::min({top, right, bottom, left});
	if (minLen < 0.05 * static_cast<double>(std::min(imageSize.width, imageSize.height))) {
		return false;
	}

	const double widthEstimate  = 0.5 * (top + bottom);
	const double heightEstimate = 0.5 * (left + right);
	const double aspect         = std::min(widthEstimate, heightEstimate) / std::max(widthEstimate, heightEstimate);
	if (aspect < 0.25) {
		return false;
	}

	const double topBottomRatio = std::min(top, bottom) / std::max(top, bottom);
	const double leftRightRatio = std::min(left, right) / std::max(left, right);
	if (topBottomRatio < 0.20 || leftRightRatio < 0.20) {
		return false;
	}

	const double parallelTopBottom = parallelCosine(quad[1] - quad[0], quad[2] - quad[3]);
	const double parallelLeftRight = parallelCosine(quad[2] - quad[1], quad[3] - quad[0]);
	if (parallelTopBottom < 0.45 || parallelLeftRight < 0.45) {
		return false;
	}

	const double c0 = cornerCosine(quad[3], quad[0], quad[1]);
	const double c1 = cornerCosine(quad[0], quad[1], quad[2]);
	const double c2 = cornerCosine(quad[1], quad[2], quad[3]);
	const double c3 = cornerCosine(quad[2], quad[3], quad[0]);
	if (std::max({c0, c1, c2, c3}) > 0.995) {
		return false;
	}

	const double borderTol = std::max(4.0, 0.01 * static_cast<double>(std::min(imageSize.width, imageSize.height)));
	int nearBorderCorners  = 0;
	for (const auto& p: quad) {
		if (p.x <= borderTol || p.y <= borderTol || p.x >= static_cast<double>(imageSize.width - 1) - borderTol ||
		    p.y >= static_cast<double>(imageSize.height - 1) - borderTol) {
			++nearBorderCorners;
		}
	}
	if (nearBorderCorners >= 4) {
		return false;
	}

	return true;
}

//! Score a plausible quad; larger is better.
static double boardQuadScore(const std::vector<cv::Point2f>& quad, const cv::Size imageSize, bool fromApprox) {
	const double imageArea = static_cast<double>(imageSize.width) * static_cast<double>(imageSize.height);
	const double areaFrac  = std::abs(cv::contourArea(quad)) / imageArea;

	const double top    = cv::norm(quad[1] - quad[0]);
	const double right  = cv::norm(quad[2] - quad[1]);
	const double bottom = cv::norm(quad[2] - quad[3]);
	const double left   = cv::norm(quad[3] - quad[0]);

	const double widthEstimate  = 0.5 * (top + bottom);
	const double heightEstimate = 0.5 * (left + right);
	const double squareness     = std::min(widthEstimate, heightEstimate) / std::max(widthEstimate, heightEstimate);
	const double topBottomRatio = std::min(top, bottom) / std::max(top, bottom);
	const double leftRightRatio = std::min(left, right) / std::max(left, right);
	const double edgeBalance    = topBottomRatio * leftRightRatio;
	const double parallelScore  = 0.5 * (parallelCosine(quad[1] - quad[0], quad[2] - quad[3]) + parallelCosine(quad[2] - quad[1], quad[3] - quad[0]));

	const double c0        = cornerCosine(quad[3], quad[0], quad[1]);
	const double c1        = cornerCosine(quad[0], quad[1], quad[2]);
	const double c2        = cornerCosine(quad[1], quad[2], quad[3]);
	const double c3        = cornerCosine(quad[2], quad[3], quad[0]);
	const double rightness = 1.0 - std::max({c0, c1, c2, c3});

	const cv::Point2f center = 0.25f * (quad[0] + quad[1] + quad[2] + quad[3]);
	const cv::Point2f imageCenter{0.5f * static_cast<float>(imageSize.width - 1), 0.5f * static_cast<float>(imageSize.height - 1)};
	const double centerDist  = cv::norm(center - imageCenter);
	const double diag        = std::sqrt(static_cast<double>(imageSize.width) * imageSize.width + static_cast<double>(imageSize.height) * imageSize.height);
	const double centerScore = std::clamp(1.0 - centerDist / (0.60 * diag), 0.0, 1.0);

	const double approxBonus = fromApprox ? 0.15 : 0.0;
	return 1.2 * areaFrac + 1.3 * squareness + 1.2 * edgeBalance + 1.1 * parallelScore + 0.6 * rightness + 0.8 * centerScore + approxBonus;
}

//! Selected board candidate including source contour index and score.
struct BoardCandidate {
	std::vector<cv::Point2f> quad;
	int contourIdx{-1};
	double score{-std::numeric_limits<double>::infinity()};
	double area{0.0};
	int verticalCount{0};
	int horizontalCount{0};
};

//! Select best board candidate from contour set using constraints + scoring.
static std::optional<BoardCandidate> selectBestBoardCandidate(const std::vector<std::vector<cv::Point>>& contours, const cv::Mat& image) {
	if (contours.empty()) {
		return std::nullopt;
	}
	const cv::Size imageSize = image.size();

	std::vector<int> sortedIndices(contours.size());
	for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
		sortedIndices[static_cast<std::size_t>(i)] = i;
	}
	std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int left, int right) {
		return std::abs(cv::contourArea(contours[static_cast<std::size_t>(left)])) > std::abs(cv::contourArea(contours[static_cast<std::size_t>(right)]));
	});

	const int candidateCount = std::min(static_cast<int>(sortedIndices.size()), MAX_CANDIDATE_CONTOURS);
	const bool verbose       = boardDebugEnabled();
	if (verbose) {
		std::cout << "[board-debug] candidate-count=" << candidateCount << '\n';
	}

	std::vector<BoardCandidate> candidates;
	candidates.reserve(static_cast<std::size_t>(candidateCount));

	for (int rank = 0; rank < candidateCount; ++rank) {
		const int contourIdx    = sortedIndices[static_cast<std::size_t>(rank)];
		const auto& contour     = contours[static_cast<std::size_t>(contourIdx)];
		const QuadCandidate q   = contourToQuad(contour);
		const bool plausible    = isPlausibleBoardQuad(q.quad, imageSize);
		const double candidateA = std::abs(cv::contourArea(q.quad));
		if (!plausible) {
			if (verbose) {
				std::cout << "[board-debug] rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea << " reject=plausibility\n";
			}
			continue;
		}

		const double quadArea  = std::max(1.0, candidateA);
		const double rectFill  = q.contourArea / quadArea;
		const bool lowRectFill = !q.fromApprox && rectFill < 0.08;
		if (lowRectFill) {
			if (verbose) {
				std::cout << "[board-debug] rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea << " quadArea=" << quadArea
				          << " fill=" << rectFill << " reject=fill\n";
			}
			continue;
		}

		const double score = boardQuadScore(q.quad, imageSize, q.fromApprox) + (q.fromApprox ? 0.45 : 0.0) + 0.90 * rectFill;
		if (verbose) {
			std::cout << "[board-debug] rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea << " quadArea=" << quadArea
			          << " fill=" << rectFill << " approx=" << (q.fromApprox ? 1 : 0) << " score=" << score << '\n';
		}

		candidates.push_back({q.quad, contourIdx, score, candidateA, 0, 0});
	}

	if (candidates.empty()) {
		return std::nullopt;
	}

	std::sort(candidates.begin(), candidates.end(), [](const BoardCandidate& left, const BoardCandidate& right) { return left.score > right.score; });

	static constexpr int REFINED_CANDIDATES = 6;
	static constexpr double GRID_SCORE_W    = 1.25;

	BoardCandidate best   = candidates.front();
	double bestFinalScore = -std::numeric_limits<double>::infinity();
	const int refineCount = std::min<int>(static_cast<int>(candidates.size()), REFINED_CANDIDATES);
	for (int i = 0; i < refineCount; ++i) {
		BoardCandidate current      = candidates[static_cast<std::size_t>(i)];
		const GridEvidence evidence = evaluateGridEvidence(image, current.quad);
		current.verticalCount       = evidence.verticalCount;
		current.horizontalCount     = evidence.horizontalCount;
		const double finalScore     = current.score + GRID_SCORE_W * evidence.score;
		if (verbose) {
			std::cout << "[board-debug] refine idx=" << current.contourIdx << " geom=" << current.score << " grid=" << evidence.score << " final=" << finalScore
			          << " v=" << evidence.verticalCount << " h=" << evidence.horizontalCount << '\n';
		}
		if (finalScore > bestFinalScore) {
			bestFinalScore = finalScore;
			best           = current;
			best.score     = finalScore;
		}
	}

	return best;
}

//! Append both external and tree-retrieval contours from one binary mask.
static void appendContours(const cv::Mat& binaryMask, std::vector<std::vector<cv::Point>>& outMerged, std::vector<std::vector<cv::Point>>* outExternal) {
	std::vector<std::vector<cv::Point>> externalContours;
	cv::Mat externalInput = binaryMask.clone();
	cv::findContours(externalInput, externalContours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	outMerged.insert(outMerged.end(), externalContours.begin(), externalContours.end());
	if (outExternal != nullptr) {
		outExternal->insert(outExternal->end(), externalContours.begin(), externalContours.end());
	}

	std::vector<std::vector<cv::Point>> allContours;
	cv::Mat allInput = binaryMask.clone();
	cv::findContours(allInput, allContours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
	outMerged.insert(outMerged.end(), allContours.begin(), allContours.end());
}

} // namespace

//! Find the board in an image and crop/scale/rectify so the image is of a planar board.
WarpResult warpToBoard(const cv::Mat& image, DebugVisualizer* debugger) {
	const auto fail = [&](const std::string& message) -> WarpResult {
		std::cerr << message << '\n';
		if (debugger) {
			debugger->endStage();
		}
		return {};
	};

	if (debugger) {
		debugger->beginStage("Warp To Board");
		debugger->add("Input", image);
	}

	if (image.empty()) {
		return fail("Failed to load image");
	}

	cv::Mat gray;
	if (!convertToGray(image, gray)) {
		return fail("Unsupported input channel count");
	}
	if (debugger)
		debugger->add("Grayscale", gray);

	const PreprocessSettings settings = choosePreprocessSettings(image.size());

	cv::Mat blurred;
	cv::GaussianBlur(gray, blurred, cv::Size(settings.blurKernelSize, settings.blurKernelSize), 1.5);
	if (debugger)
		debugger->add("Gaussian Blur", blurred);

	const CandidateMasks masks = buildCandidateMasks(blurred, settings);
	if (debugger) {
		debugger->add("Edge Mask", masks.edgeMask);
		debugger->add("Bright Mask", masks.brightMask);
		debugger->add("Dark Mask", masks.darkMask);
	}

	std::vector<std::vector<cv::Point>> contours;
	std::vector<std::vector<cv::Point>> contoursExternal;
	appendContours(masks.edgeMask, contours, &contoursExternal);
	appendContours(masks.brightMask, contours, &contoursExternal);
	appendContours(masks.darkMask, contours, &contoursExternal);
	if (contours.empty()) {
		return fail("No contours found");
	}

	if (debugger) {
		cv::Mat drawnContours = image.clone();
		cv::drawContours(drawnContours, contoursExternal, -1, cv::Scalar(255, 0, 0), 2);
		debugger->add("Contour Finder", drawnContours);
	}

	const auto bestCandidate = selectBestBoardCandidate(contours, image);
	if (!bestCandidate.has_value()) {
		return fail("No valid board candidate found");
	}

	std::cout << "Largest contour idx: " << bestCandidate->contourIdx << " area: " << bestCandidate->area << "\n";
	if (debugger) {
		cv::Mat selected = image.clone();
		std::vector<cv::Point> poly;
		poly.reserve(bestCandidate->quad.size());
		for (const auto& p: bestCandidate->quad) {
			poly.emplace_back(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
		}
		cv::polylines(selected, poly, true, cv::Scalar(0, 255, 0), 3);
		debugger->add("Contour Selected", selected);
	}

	const std::vector<cv::Point2f> dst = {
	        {0.f, 0.f},
	        {static_cast<float>(WARP_OUT_SIZE) - 1.f, 0.f},
	        {static_cast<float>(WARP_OUT_SIZE) - 1.f, static_cast<float>(WARP_OUT_SIZE) - 1.f},
	        {0.f, static_cast<float>(WARP_OUT_SIZE) - 1.f},
	};

	cv::Mat H = cv::getPerspectiveTransform(bestCandidate->quad, dst);

	cv::Mat warped;
	cv::warpPerspective(image, warped, H, cv::Size(WARP_OUT_SIZE, WARP_OUT_SIZE));
	if (debugger) {
		debugger->add("Warped", warped);
		debugger->endStage();
	}

	return {warped, H};
}

} // namespace tengen::vision::core
