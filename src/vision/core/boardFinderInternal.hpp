#pragma once

#include "vision/core/debugVisualizer.hpp"

#include <optional>
#include <vector>

#include <opencv2/geometry.hpp>
#include <opencv2/opencv.hpp>

#if defined(VISION_DEBUG_LOGGING) && defined(VISION_LOG_BOARDFINDER)
#include <iostream>
#define DEBUG_LOG(x) std::cout << x;
#else
#define DEBUG_LOG(x) ((void)0);
#endif

namespace tengen::vision::core {
namespace internal {

inline constexpr int WARP_OUT_SIZE          = 1000; //!< Normalized output size used by rough board warp.
inline constexpr int MAX_CANDIDATE_CONTOURS = 80;   //!< Number of largest contours considered for board-candidate scoring.

//! Geometric constraints used to validate board quads.
struct QuadConstraints {
	double minAreaFrac{0.08};
	double maxAreaFrac{0.75};
	double minEdgeLenFrac{0.05};
	double minAspect{0.25};
	double minTopBottomRatio{0.20};
	double minLeftRightRatio{0.20};
	double minParallelTopBottom{0.45};
	double minParallelLeftRight{0.45};
	double maxCornerCosine{0.995};
	int maxNearBorderCorners{3};
	double minRectFillForMinAreaRect{0.08};
};

//! Line-count scoring settings for legal board-size evidence.
struct LineCountScoreSettings {
	std::array<int, 3> legalBoardLineCounts{9, 13, 19};
	double distanceForZeroScore{8.0};
};

//! Settings for fast grid evidence extraction from a warped candidate.
struct GridEvidenceSettings {
	int blurKernelSize{9};
	double blurSigma{1.5};
	double cannyLow{50.0};
	double cannyHigh{120.0};
	int edgeDilateIterations{1};
	double houghRho{1.0};
	double houghThetaDeg{1.0};
	int houghThreshold{80};
	double houghMinLineLength{100.0};
	double houghMaxLineGap{20.0};
	double horizontalAngleMaxDeg{15.0};
	double verticalAngleMinDeg{75.0};
	double clusterEpsilonPx{15.0};
	double balanceDiffForZeroScore{12.0};
	double segmentSupportForFullScore{220.0};
	double pairFitWeight{2.0};
	double balanceWeight{0.8};
	double segmentSupportWeight{0.4};
};

//! Geometric scoring weights for candidate quadrilaterals.
struct QuadScoreSettings {
	double areaFracWeight{1.2};
	double squarenessWeight{1.3};
	double edgeBalanceWeight{1.2};
	double parallelWeight{1.1};
	double rightnessWeight{0.6};
	double centerWeight{0.8};
	double centerDistanceDiagFrac{0.60};
	double fromApproxBonus{0.15};
};

//! Candidate-ranking weights that combine geometry and grid evidence.
struct CandidateScoringSettings {
	double extraApproxPreference{0.45};
	double rectFillWeight{0.90};
	double minQuadAreaForRectFill{1.0};
	int refinedCandidateCount{6};
	double strictGridScoreWeight{1.25};
	double relaxedGridScoreWeight{2.0};
};

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

//! Ensure odd kernel sizes for blur/morphology operations.
constexpr int makeOddKernelSize(const int value);

//! Select preprocessing settings from image size to stay robust across resolutions.
PreprocessSettings choosePreprocessSettings(const cv::Size imageSize);

//! Convert image to grayscale independent of channel format.
bool convertToGray(const cv::Mat& image, cv::Mat& outGray);

//! Mildly enhance local contrast in warped output to stabilize downstream grid detection on faint lines.
cv::Mat enhanceWarpContrast(const cv::Mat& image);

//! Build complementary binary masks used for contour extraction.
CandidateMasks buildCandidateMasks(const cv::Mat& blurredGray, const PreprocessSettings& settings);

//! Order 4 corner points TL,TR,BR,BL.
std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point2f>& quad);

//! Compute cosine of corner angle p0-p1-p2.
double cornerCosine(const cv::Point2f& p0, const cv::Point2f& p1, const cv::Point2f& p2);

//! Compute absolute cosine between two vectors (parallel/anti-parallel -> 1).
double parallelCosine(const cv::Point2f& v0, const cv::Point2f& v1);


//! Grid evidence extracted from a warped board candidate.
struct GridEvidence {
	double score{0.0};
	int verticalCount{0};
	int horizontalCount{0};
};

//! Try multiple approximation epsilons until a convex 4-corner polygon is found.
bool contourToApproxQuad(const std::vector<cv::Point>& contour, std::vector<cv::Point2f>& outQuad);

//! Quad candidate extracted from one contour.
struct QuadCandidate {
	std::vector<cv::Point2f> quad;
	bool fromApprox{false};
	double contourArea{0.0};
};

//! Convert contour to quad using polygon approximation, then minAreaRect fallback.
QuadCandidate contourToQuad(const std::vector<cv::Point>& contour);

//! Weighted 1D line center used for line-center clustering.
struct Line1D {
	double pos{0.0};
	double weight{0.0};
};

//! Cluster close weighted line centers into single representative centers.
std::vector<double> clusterWeighted1D(std::vector<Line1D> values, double eps);

//! Score how close a line-count is to legal Go board sizes.
double boardLineCountScore(const int count, const LineCountScoreSettings& settings);

//! Evaluate grid-line evidence for one board candidate using a fast line-count check on the warped candidate.
GridEvidence evaluateGridEvidence(const cv::Mat& image, const std::vector<cv::Point2f>& quad, const GridEvidenceSettings& evidenceSettings,
                                  const LineCountScoreSettings& lineCountSettings);

//! Check whether a quad satisfies geometric constraints expected for a physical Go board.
bool isPlausibleBoardQuad(const std::vector<cv::Point2f>& quad, const cv::Size imageSize, const QuadConstraints& constraints);

//! Score a plausible quad; larger is better.
double boardQuadScore(const std::vector<cv::Point2f>& quad, const cv::Size imageSize, bool fromApprox, const QuadScoreSettings& settings);


//! Selected board candidate including source contour index and score.
struct BoardCandidate {
	std::vector<cv::Point2f> quad;
	int contourIdx{-1};
	double score{-std::numeric_limits<double>::infinity()};
	double area{0.0};
	int verticalCount{0};
	int horizontalCount{0};
	bool fromRelaxedPass{false};
};

//! Select best board candidate from contour set using constraints + scoring.
std::optional<BoardCandidate> selectBestBoardCandidate(const std::vector<std::vector<cv::Point>>& contours, const cv::Mat& image);

//! Append both external and tree-retrieval contours from one binary mask.
void appendContours(const cv::Mat& binaryMask, std::vector<std::vector<cv::Point>>& outMerged, std::vector<std::vector<cv::Point>>* outExternal);

} // namespace internal
} // namespace tengen::vision::core