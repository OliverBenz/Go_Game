#include "boardFinderInternal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace tengen::vision::core {
namespace internal {

//! Strict defaults preserve current behavior on known-good datasets.
static constexpr QuadConstraints STRICT_CONSTRAINTS{};

//! Relaxed fallback for difficult frames (e.g. near-border boards or thin outline contours).
static constexpr QuadConstraints RELAXED_CONSTRAINTS = {
        .minAreaFrac               = 0.04,
        .maxAreaFrac               = 0.995,
        .minEdgeLenFrac            = 0.03,
        .minAspect                 = 0.16,
        .minTopBottomRatio         = 0.10,
        .minLeftRightRatio         = 0.10,
        .minParallelTopBottom      = 0.25,
        .minParallelLeftRight      = 0.25,
        .maxCornerCosine           = 0.998,
        .maxNearBorderCorners      = 4,
        .minRectFillForMinAreaRect = 0.0,
};
static constexpr LineCountScoreSettings LINE_COUNT_SCORE_SETTINGS{};
static constexpr GridEvidenceSettings GRID_EVIDENCE_SETTINGS{};
static constexpr QuadScoreSettings QUAD_SCORE_SETTINGS{};
static constexpr CandidateScoringSettings CANDIDATE_SCORING_SETTINGS{};

//! Ensure odd kernel sizes for blur/morphology operations.
constexpr int makeOddKernelSize(const int value) {
	return (value % 2 == 0) ? value + 1 : value;
}

//! Select preprocessing settings from image size to stay robust across resolutions.
PreprocessSettings choosePreprocessSettings(const cv::Size imageSize) {
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
bool convertToGray(const cv::Mat& image, cv::Mat& outGray) {
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

//! Mildly enhance local contrast in warped output to stabilize downstream grid detection on faint lines.
cv::Mat enhanceWarpContrast(const cv::Mat& image) {
	if (image.empty()) {
		return image;
	}

	cv::Mat bgr;
	if (image.channels() == 3) {
		bgr = image.clone();
	} else if (image.channels() == 4) {
		cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
	} else if (image.channels() == 1) {
		cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
	} else {
		return image;
	}

	cv::Mat lab;
	cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	std::vector<cv::Mat> channels;
	cv::split(lab, channels);
	if (channels.size() != 3u) {
		return bgr;
	}

	cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
	clahe->apply(channels[0], channels[0]);
	cv::merge(channels, lab);

	cv::Mat enhanced;
	cv::cvtColor(lab, enhanced, cv::COLOR_Lab2BGR);
	return enhanced;
}

//! Build complementary binary masks used for contour extraction.
CandidateMasks buildCandidateMasks(const cv::Mat& blurredGray, const PreprocessSettings& settings) {
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
std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point2f>& quad) {
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
		        quad[static_cast<std::size_t>(idxMaxDiff)],
		        quad[static_cast<std::size_t>(idxMaxSum)],
		        quad[static_cast<std::size_t>(idxMinDiff)],
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
double cornerCosine(const cv::Point2f& p0, const cv::Point2f& p1, const cv::Point2f& p2) {
	const cv::Point2f v1 = p0 - p1;
	const cv::Point2f v2 = p2 - p1;
	const double norm1   = cv::norm(v1);
	const double norm2   = cv::norm(v2);
	if (norm1 <= 1e-6 || norm2 <= 1e-6) {
		return 1.0;
	}
	return std::abs((v1.x * v2.x + v1.y * v2.y) / static_cast<float>(norm1 * norm2));
}

//! Compute absolute cosine between two vectors (parallel/anti-parallel -> 1).
double parallelCosine(const cv::Point2f& v0, const cv::Point2f& v1) {
	const double norm0 = cv::norm(v0);
	const double norm1 = cv::norm(v1);
	if (norm0 <= 1e-6 || norm1 <= 1e-6) {
		return 0.0;
	}
	return std::abs((v0.x * v1.x + v0.y * v1.y) / static_cast<float>(norm0 * norm1));
}

//! Try multiple approximation epsilons until a convex 4-corner polygon is found.
bool contourToApproxQuad(const std::vector<cv::Point>& contour, std::vector<cv::Point2f>& outQuad) {
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
QuadCandidate contourToQuad(const std::vector<cv::Point>& contour) {
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
std::vector<double> clusterWeighted1D(std::vector<Line1D> values, double eps) {
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
double boardLineCountScore(const int count, const LineCountScoreSettings& settings) {
	int best = std::numeric_limits<int>::max();
	for (const int legalCount: settings.legalBoardLineCounts) {
		best = std::min(best, std::abs(count - legalCount));
	}
	if (best == std::numeric_limits<int>::max()) {
		return 0.0;
	}
	return std::clamp(1.0 - static_cast<double>(best) / settings.distanceForZeroScore, 0.0, 1.0);
}

//! Evaluate grid-line evidence for one board candidate using a fast line-count check on the warped candidate.
GridEvidence evaluateGridEvidence(const cv::Mat& image, const std::vector<cv::Point2f>& quad, const GridEvidenceSettings& evidenceSettings,
                                  const LineCountScoreSettings& lineCountSettings) {
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
	cv::GaussianBlur(gray, blur, cv::Size(evidenceSettings.blurKernelSize, evidenceSettings.blurKernelSize), evidenceSettings.blurSigma);

	cv::Mat edges;
	cv::Canny(blur, edges, evidenceSettings.cannyLow, evidenceSettings.cannyHigh);
	cv::dilate(edges, edges, cv::Mat(), cv::Point(-1, -1), evidenceSettings.edgeDilateIterations);

	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines, evidenceSettings.houghRho, evidenceSettings.houghThetaDeg * CV_PI / 180.0, evidenceSettings.houghThreshold,
	                evidenceSettings.houghMinLineLength, evidenceSettings.houghMaxLineGap);
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

		if (std::abs(angle) < evidenceSettings.horizontalAngleMaxDeg) {
			horizontal.push_back({0.5 * static_cast<double>(line[1] + line[3]), segLen});
		} else if (std::abs(angle) > evidenceSettings.verticalAngleMinDeg) {
			vertical.push_back({0.5 * static_cast<double>(line[0] + line[2]), segLen});
		}
	}

	const std::vector<double> vCenters = clusterWeighted1D(vertical, evidenceSettings.clusterEpsilonPx);
	const std::vector<double> hCenters = clusterWeighted1D(horizontal, evidenceSettings.clusterEpsilonPx);

	evidence.verticalCount   = static_cast<int>(vCenters.size());
	evidence.horizontalCount = static_cast<int>(hCenters.size());

	const double fitV    = boardLineCountScore(evidence.verticalCount, lineCountSettings);
	const double fitH    = boardLineCountScore(evidence.horizontalCount, lineCountSettings);
	const double pairFit = std::sqrt(fitV * fitH);
	const double balance = std::clamp(1.0 - std::abs(evidence.verticalCount - evidence.horizontalCount) / evidenceSettings.balanceDiffForZeroScore, 0.0, 1.0);
	const double segmentSupport =
	        std::clamp((static_cast<double>(vertical.size()) + static_cast<double>(horizontal.size())) / evidenceSettings.segmentSupportForFullScore, 0.0, 1.0);

	evidence.score =
	        evidenceSettings.pairFitWeight * pairFit + evidenceSettings.balanceWeight * balance + evidenceSettings.segmentSupportWeight * segmentSupport;
	return evidence;
}

//! Check whether a quad satisfies geometric constraints expected for a physical Go board.
bool isPlausibleBoardQuad(const std::vector<cv::Point2f>& quad, const cv::Size imageSize, const QuadConstraints& constraints) {
	if (quad.size() != 4u) {
		return false;
	}

	const double imageArea = static_cast<double>(imageSize.width) * static_cast<double>(imageSize.height);
	const double area      = std::abs(cv::contourArea(quad));
	if (area < constraints.minAreaFrac * imageArea) {
		return false;
	}
	if (area > constraints.maxAreaFrac * imageArea) {
		return false;
	}

	const double top    = cv::norm(quad[1] - quad[0]);
	const double right  = cv::norm(quad[2] - quad[1]);
	const double bottom = cv::norm(quad[2] - quad[3]);
	const double left   = cv::norm(quad[3] - quad[0]);
	const double minLen = std::min({top, right, bottom, left});
	if (minLen < constraints.minEdgeLenFrac * static_cast<double>(std::min(imageSize.width, imageSize.height))) {
		return false;
	}

	const double widthEstimate  = 0.5 * (top + bottom);
	const double heightEstimate = 0.5 * (left + right);
	const double aspect         = std::min(widthEstimate, heightEstimate) / std::max(widthEstimate, heightEstimate);
	if (aspect < constraints.minAspect) {
		return false;
	}

	const double topBottomRatio = std::min(top, bottom) / std::max(top, bottom);
	const double leftRightRatio = std::min(left, right) / std::max(left, right);
	if (topBottomRatio < constraints.minTopBottomRatio || leftRightRatio < constraints.minLeftRightRatio) {
		return false;
	}

	const double parallelTopBottom = parallelCosine(quad[1] - quad[0], quad[2] - quad[3]);
	const double parallelLeftRight = parallelCosine(quad[2] - quad[1], quad[3] - quad[0]);
	if (parallelTopBottom < constraints.minParallelTopBottom || parallelLeftRight < constraints.minParallelLeftRight) {
		return false;
	}

	const double c0 = cornerCosine(quad[3], quad[0], quad[1]);
	const double c1 = cornerCosine(quad[0], quad[1], quad[2]);
	const double c2 = cornerCosine(quad[1], quad[2], quad[3]);
	const double c3 = cornerCosine(quad[2], quad[3], quad[0]);
	if (std::max({c0, c1, c2, c3}) > constraints.maxCornerCosine) {
		return false;
	}

	const float borderTol = std::max(4.0f, 0.01f * static_cast<float>(std::min(imageSize.width, imageSize.height)));
	int nearBorderCorners = 0;
	for (const auto& p: quad) {
		if (p.x <= borderTol || p.y <= borderTol || p.x >= static_cast<float>(imageSize.width - 1) - borderTol ||
		    p.y >= static_cast<float>(imageSize.height - 1) - borderTol) {
			++nearBorderCorners;
		}
	}
	if (nearBorderCorners > constraints.maxNearBorderCorners) {
		return false;
	}

	return true;
}

//! Score a plausible quad; larger is better.
double boardQuadScore(const std::vector<cv::Point2f>& quad, const cv::Size imageSize, bool fromApprox, const QuadScoreSettings& settings) {
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
	const double centerScore = std::clamp(1.0 - centerDist / (settings.centerDistanceDiagFrac * diag), 0.0, 1.0);

	const double approxBonus = fromApprox ? settings.fromApproxBonus : 0.0;
	return settings.areaFracWeight * areaFrac + settings.squarenessWeight * squareness + settings.edgeBalanceWeight * edgeBalance +
	       settings.parallelWeight * parallelScore + settings.rightnessWeight * rightness + settings.centerWeight * centerScore + approxBonus;
}

//! Select best board candidate from contour set using constraints + scoring.
std::optional<BoardCandidate> selectBestBoardCandidate(const std::vector<std::vector<cv::Point>>& contours, const cv::Mat& image) {
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
	DEBUG_LOG("[board-debug] candidate-count=" << candidateCount << '\n');

	const auto collectCandidates = [&](const QuadConstraints& constraints, const char* passName, const bool fromRelaxedPass) {
		(void)passName;
		std::vector<BoardCandidate> candidates;
		candidates.reserve(static_cast<std::size_t>(candidateCount));

		for (int rank = 0; rank < candidateCount; ++rank) {
			const int contourIdx    = sortedIndices[static_cast<std::size_t>(rank)];
			const auto& contour     = contours[static_cast<std::size_t>(contourIdx)];
			const QuadCandidate q   = contourToQuad(contour);
			const bool plausible    = isPlausibleBoardQuad(q.quad, imageSize, constraints);
			const double candidateA = std::abs(cv::contourArea(q.quad));
			if (!plausible) {
				DEBUG_LOG("[board-debug] pass=" << passName << " rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea
				                                << " reject=plausibility\n");
				continue;
			}

			const double quadArea  = std::max(CANDIDATE_SCORING_SETTINGS.minQuadAreaForRectFill, candidateA);
			const double rectFill  = q.contourArea / quadArea;
			const bool lowRectFill = !q.fromApprox && rectFill < constraints.minRectFillForMinAreaRect;
			if (lowRectFill) {
				DEBUG_LOG("[board-debug] pass=" << passName << " rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea
				                                << " quadArea=" << quadArea << " fill=" << rectFill << " reject=fill\n");
				continue;
			}

			const double score = boardQuadScore(q.quad, imageSize, q.fromApprox, QUAD_SCORE_SETTINGS) +
			                     (q.fromApprox ? CANDIDATE_SCORING_SETTINGS.extraApproxPreference : 0.0) + CANDIDATE_SCORING_SETTINGS.rectFillWeight * rectFill;
			DEBUG_LOG("[board-debug] pass=" << passName << " rank=" << rank << " idx=" << contourIdx << " contourArea=" << q.contourArea << " quadArea="
			                                << quadArea << " fill=" << rectFill << " approx=" << (q.fromApprox ? 1 : 0) << " score=" << score << '\n');

			candidates.push_back({q.quad, contourIdx, score, candidateA, 0, 0, fromRelaxedPass});
		}

		return candidates;
	};

	std::vector<BoardCandidate> candidates = collectCandidates(STRICT_CONSTRAINTS, "strict", false);
	bool usedRelaxedPass                   = false;
	if (candidates.empty()) {
		DEBUG_LOG("[board-debug] strict pass empty; trying relaxed pass\n");
		candidates      = collectCandidates(RELAXED_CONSTRAINTS, "relaxed", true);
		usedRelaxedPass = true;
	}

	if (candidates.empty()) {
		return std::nullopt;
	}

	std::sort(candidates.begin(), candidates.end(), [](const BoardCandidate& left, const BoardCandidate& right) { return left.score > right.score; });

	const double gridScoreWeight = usedRelaxedPass ? CANDIDATE_SCORING_SETTINGS.relaxedGridScoreWeight : CANDIDATE_SCORING_SETTINGS.strictGridScoreWeight;

	BoardCandidate best   = candidates.front();
	double bestFinalScore = -std::numeric_limits<double>::infinity();
	const int refineCount = std::min<int>(static_cast<int>(candidates.size()), CANDIDATE_SCORING_SETTINGS.refinedCandidateCount);
	for (int i = 0; i < refineCount; ++i) {
		BoardCandidate current      = candidates[static_cast<std::size_t>(i)];
		const GridEvidence evidence = evaluateGridEvidence(image, current.quad, GRID_EVIDENCE_SETTINGS, LINE_COUNT_SCORE_SETTINGS);
		current.verticalCount       = evidence.verticalCount;
		current.horizontalCount     = evidence.horizontalCount;
		const double finalScore     = current.score + gridScoreWeight * evidence.score;
		DEBUG_LOG("[board-debug] refine idx=" << current.contourIdx << " geom=" << current.score << " grid=" << evidence.score << " final=" << finalScore
		                                      << " v=" << evidence.verticalCount << " h=" << evidence.horizontalCount << '\n');
		if (finalScore > bestFinalScore) {
			bestFinalScore = finalScore;
			best           = current;
			best.score     = finalScore;
		}
	}

	return best;
}

//! Append both external and tree-retrieval contours from one binary mask.
void appendContours(const cv::Mat& binaryMask, std::vector<std::vector<cv::Point>>& outMerged, std::vector<std::vector<cv::Point>>* outExternal) {
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

} // namespace internal
} // namespace tengen::vision::core