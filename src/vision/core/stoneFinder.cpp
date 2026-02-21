#include "camera/stoneFinder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

namespace go::vision::core {

namespace {

// Stone detection pipeline architecture:
// 1) GeometrySampling   -> derive ROI radii and pixel offsets from board spacing.
// 2) FeatureExtraction  -> compute per-intersection Lab/background/support features.
// 3) ModelCalibration   -> robustly estimate empty-board statistics for this frame.
// 4) Scoring            -> map features to raw Black/White/Empty scores.
// 5) DecisionPolicy     -> apply domain gating and acceptance rules to raw evals.
// 6) RefinementEngine   -> search local offsets for better raw candidates when asked.
// 7) Debugging          -> emit overlays and optional runtime diagnostics.

static int roundedSpacingValue(double spacing, double scale, int fallback, int minValue, int maxValue) {
	int value = fallback;
	if (std::isfinite(spacing) && spacing > 0.0) {
		value = static_cast<int>(std::lround(spacing * scale));
	}
	return std::clamp(value, minValue, maxValue);
}

struct Radii {
	int innerRadius{0};
	int bgRadius{0};
	int bgOffset{0};
};

struct Offsets {
	std::vector<cv::Point> inner;
	std::vector<cv::Point> bg;
};

struct LabBlur {
	cv::Mat L;
	cv::Mat A;
	cv::Mat B;
};

struct SampleContext {
	const cv::Mat& L;
	const cv::Mat& A;
	const cv::Mat& B;
	int rows{0};
	int cols{0};
};

struct Features {
	float deltaL{0.0f};
	float chromaSq{0.0f};
	float darkFrac{0.0f};
	float brightFrac{0.0f};
	bool valid{false};
};

struct Model {
	float medianEmpty{0.0f};
	float sigmaEmpty{1.0f};
	float tChromaSq{0.0f};
};

struct Scores {
	float black{0.0f};
	float white{0.0f};
	float empty{0.0f};
	float chromaPenalty{0.0f};
	float z{0.0f};
};

struct Eval {
	StoneState state{StoneState::Empty};
	float bestScore{0.0f};
	float secondScore{0.0f};
	float margin{0.0f};
	float required{0.0f};
	float confidence{0.0f};
	float z{0.0f};
};

struct SpatialContext {
	int edgeLevel{0};
	float neighborMedian{0.0f};
	unsigned boardSize{0u};
};

enum class RejectionReason {
	None,
	WeakZ,
	LowConfidence,
	WeakSupport,
	WeakNeighborContrast,
	EdgeArtifact,
	MarginTooSmall,
	Other,
};

struct DebugStats {
	int blackCount{0};
	int whiteCount{0};
	int emptyCount{0};
	int refinedTried{0};
	int refinedAccepted{0};
};

namespace GeometrySampling {

static Radii chooseRadii(double spacing, const GeometryConfig& config) {
	Radii radii{};
	radii.innerRadius = roundedSpacingValue(spacing, config.innerRadiusSpacingK, config.innerRadiusFallback, config.innerRadiusMin, config.innerRadiusMax);
	radii.bgRadius    = std::clamp(radii.innerRadius / 2, config.bgRadiusMin, config.bgRadiusMax);

	if (std::isfinite(spacing) && spacing > 0.0) {
		const int bgOffset = static_cast<int>(std::lround(spacing * config.bgOffsetSpacingK));
		radii.bgOffset     = std::max(bgOffset, radii.innerRadius + config.bgOffsetMinExtra);
	} else {
		radii.bgOffset = radii.innerRadius * 2 + config.bgOffsetFallbackAdd;
	}
	return radii;
}

static std::vector<cv::Point> makeCircleOffsets(int radius) {
	std::vector<cv::Point> offsets;
	offsets.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
	const int radiusSquared = radius * radius;
	for (int deltaY = -radius; deltaY <= radius; ++deltaY) {
		for (int deltaX = -radius; deltaX <= radius; ++deltaX) {
			if (deltaX * deltaX + deltaY * deltaY <= radiusSquared) {
				offsets.emplace_back(deltaX, deltaY);
			}
		}
	}
	return offsets;
}

static Offsets precomputeOffsets(const Radii& radii) {
	Offsets offsets{};
	offsets.inner = makeCircleOffsets(radii.innerRadius);
	offsets.bg    = (radii.bgRadius == radii.innerRadius) ? offsets.inner : makeCircleOffsets(radii.bgRadius);
	return offsets;
}

} // namespace GeometrySampling

namespace FeatureExtraction {

static bool convertToLab(const cv::Mat& image, cv::Mat& outLab) {
	if (image.channels() == 3) {
		cv::cvtColor(image, outLab, cv::COLOR_BGR2Lab);
		return true;
	}

	if (image.channels() == 4) {
		cv::Mat bgrImage;
		cv::cvtColor(image, bgrImage, cv::COLOR_BGRA2BGR);
		cv::cvtColor(bgrImage, outLab, cv::COLOR_BGR2Lab);
		return true;
	}

	if (image.channels() == 1) {
		cv::Mat bgrImage;
		cv::cvtColor(image, bgrImage, cv::COLOR_GRAY2BGR);
		cv::cvtColor(bgrImage, outLab, cv::COLOR_BGR2Lab);
		return true;
	}

	return false;
}

static bool prepareLabBlur(const cv::Mat& image, const Radii& radii, const GeometryConfig& config, LabBlur& outBlur) {
	cv::Mat labImage;
	if (!convertToLab(image, labImage)) {
		return false;
	}

	cv::extractChannel(labImage, outBlur.L, 0);
	cv::extractChannel(labImage, outBlur.A, 1);
	cv::extractChannel(labImage, outBlur.B, 2);

	const double sigma = std::clamp(config.blurSigmaRadiusK * static_cast<double>(radii.innerRadius), config.blurSigmaMin, config.blurSigmaMax);

	cv::GaussianBlur(outBlur.L, outBlur.L, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	cv::GaussianBlur(outBlur.A, outBlur.A, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	cv::GaussianBlur(outBlur.B, outBlur.B, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	return true;
}

static bool sampleMeanL(const SampleContext& context, int centerX, int centerY, const std::vector<cv::Point>& offsets, float& outMean) {
	std::uint32_t sum   = 0u;
	std::uint32_t count = 0u;
	for (const cv::Point& offset: offsets) {
		const int x = centerX + offset.x;
		const int y = centerY + offset.y;
		if (x < 0 || x >= context.cols || y < 0 || y >= context.rows) {
			continue;
		}
		sum += static_cast<std::uint32_t>(context.L.ptr<std::uint8_t>(y)[x]);
		++count;
	}
	if (count == 0u) {
		return false;
	}
	outMean = static_cast<float>(sum) / static_cast<float>(count);
	return true;
}

static bool sampleMeanLab(const SampleContext& context, int centerX, int centerY, const std::vector<cv::Point>& offsets, float& outL, float& outA,
                          float& outB) {
	std::uint32_t sumL  = 0u;
	std::uint32_t sumA  = 0u;
	std::uint32_t sumB  = 0u;
	std::uint32_t count = 0u;
	for (const cv::Point& offset: offsets) {
		const int x = centerX + offset.x;
		const int y = centerY + offset.y;
		if (x < 0 || x >= context.cols || y < 0 || y >= context.rows) {
			continue;
		}
		sumL += static_cast<std::uint32_t>(context.L.ptr<std::uint8_t>(y)[x]);
		sumA += static_cast<std::uint32_t>(context.A.ptr<std::uint8_t>(y)[x]);
		sumB += static_cast<std::uint32_t>(context.B.ptr<std::uint8_t>(y)[x]);
		++count;
	}
	if (count == 0u) {
		return false;
	}

	outL = static_cast<float>(sumL) / static_cast<float>(count);
	outA = static_cast<float>(sumA) / static_cast<float>(count);
	outB = static_cast<float>(sumB) / static_cast<float>(count);
	return true;
}

static bool computeFeaturesAt(const SampleContext& context, const Offsets& offsets, const Radii& radii, const GeometryConfig& config, int centerX, int centerY,
                              Features& outFeatures) {
	outFeatures = Features{};

	float innerL = 0.0f;
	float innerA = 0.0f;
	float innerB = 0.0f;
	if (!sampleMeanLab(context, centerX, centerY, offsets.inner, innerL, innerA, innerB)) {
		return false;
	}

	std::array<float, 8> backgroundSamples{};
	int backgroundCount  = 0;
	float backgroundMean = 0.0f;
	const std::array<cv::Point, 8> directions{
	        cv::Point(-1, -1), cv::Point(1, -1), cv::Point(-1, 1), cv::Point(1, 1), cv::Point(-1, 0), cv::Point(1, 0), cv::Point(0, -1), cv::Point(0, 1),
	};
	for (const cv::Point& direction: directions) {
		const int sampleX = centerX + direction.x * radii.bgOffset;
		const int sampleY = centerY + direction.y * radii.bgOffset;
		if (sampleMeanL(context, sampleX, sampleY, offsets.bg, backgroundMean)) {
			backgroundSamples[static_cast<std::size_t>(backgroundCount++)] = backgroundMean;
		}
	}
	if (backgroundCount < config.minBgSamples) {
		return false;
	}

	std::sort(backgroundSamples.begin(), backgroundSamples.begin() + backgroundCount);
	const float backgroundMedian = (backgroundCount % 2 == 1) ? backgroundSamples[static_cast<std::size_t>(backgroundCount / 2)]
	                                                          : 0.5f * (backgroundSamples[static_cast<std::size_t>(backgroundCount / 2 - 1)] +
	                                                                    backgroundSamples[static_cast<std::size_t>(backgroundCount / 2)]);

	outFeatures.deltaL   = innerL - backgroundMedian;
	const float deltaA   = innerA - config.labNeutral;
	const float deltaB   = innerB - config.labNeutral;
	outFeatures.chromaSq = deltaA * deltaA + deltaB * deltaB;

	std::uint32_t total  = 0u;
	std::uint32_t dark   = 0u;
	std::uint32_t bright = 0u;
	for (const cv::Point& offset: offsets.inner) {
		const int x = centerX + offset.x;
		const int y = centerY + offset.y;
		if (x < 0 || x >= context.cols || y < 0 || y >= context.rows) {
			continue;
		}
		const float diff = static_cast<float>(context.L.ptr<std::uint8_t>(y)[x]) - backgroundMedian;
		++total;
		if (diff <= -config.supportDelta) {
			++dark;
		} else if (diff >= config.supportDelta) {
			++bright;
		}
	}

	if (total > 0u) {
		outFeatures.darkFrac   = static_cast<float>(dark) / static_cast<float>(total);
		outFeatures.brightFrac = static_cast<float>(bright) / static_cast<float>(total);
	}

	outFeatures.valid = true;
	return true;
}

static std::vector<Features> computeFeatures(const std::vector<cv::Point2f>& intersections, const SampleContext& context, const Offsets& offsets,
                                             const Radii& radii, const GeometryConfig& config) {
	std::vector<Features> allFeatures(intersections.size());
	for (std::size_t index = 0; index < intersections.size(); ++index) {
		const int centerX = static_cast<int>(std::lround(intersections[index].x));
		const int centerY = static_cast<int>(std::lround(intersections[index].y));
		computeFeaturesAt(context, offsets, radii, config, centerX, centerY, allFeatures[index]);
	}
	return allFeatures;
}

} // namespace FeatureExtraction

namespace ModelCalibration {

static float medianSorted(const std::vector<float>& sortedValues) {
	const std::size_t count = sortedValues.size();
	if (count == 0u) {
		return 0.0f;
	}
	return (count % 2u == 1u) ? sortedValues[(count - 1u) / 2u] : 0.5f * (sortedValues[count / 2u - 1u] + sortedValues[count / 2u]);
}

static bool robustMedianSigma(const std::vector<float>& values, const CalibrationConfig& config, float& outMedian, float& outSigma) {
	if (values.empty()) {
		return false;
	}

	std::vector<float> sortedValues = values;
	std::sort(sortedValues.begin(), sortedValues.end());
	outMedian = medianSorted(sortedValues);

	std::vector<float> absoluteDeviation;
	absoluteDeviation.reserve(sortedValues.size());
	for (float value: sortedValues) {
		absoluteDeviation.push_back(std::abs(value - outMedian));
	}
	std::sort(absoluteDeviation.begin(), absoluteDeviation.end());

	const float mad = medianSorted(absoluteDeviation);
	outSigma        = std::max(config.sigmaMin, config.madToSigma * mad);
	return true;
}

static bool calibrateModel(const std::vector<Features>& features, unsigned boardSize, const CalibrationConfig& calibrationConfig, Model& outModel) {
	std::vector<float> allDelta;
	std::vector<float> allChroma;
	allDelta.reserve(features.size());
	allChroma.reserve(features.size());
	for (const Features& feature: features) {
		if (!feature.valid) {
			continue;
		}
		allDelta.push_back(feature.deltaL);
		allChroma.push_back(feature.chromaSq);
	}
	if (allDelta.empty()) {
		return false;
	}

	float medianInitial = 0.0f;
	float sigmaInitial  = calibrationConfig.sigmaMin;
	if (!robustMedianSigma(allDelta, calibrationConfig, medianInitial, sigmaInitial)) {
		return false;
	}

	const float emptyBand = calibrationConfig.emptyBandSigma * sigmaInitial;
	std::vector<float> likelyEmptyDelta;
	std::vector<float> likelyEmptyChroma;
	likelyEmptyDelta.reserve(allDelta.size());
	likelyEmptyChroma.reserve(allChroma.size());
	for (const Features& feature: features) {
		if (!feature.valid) {
			continue;
		}
		const bool inBand     = std::abs(feature.deltaL - medianInitial) <= emptyBand;
		const bool lowSupport = (feature.darkFrac + feature.brightFrac) <= calibrationConfig.likelyEmptySupportSumMax;
		if (inBand && lowSupport) {
			likelyEmptyDelta.push_back(feature.deltaL);
			likelyEmptyChroma.push_back(feature.chromaSq);
		}
	}

	const std::size_t minEmptyCount = std::max<std::size_t>(
	        static_cast<std::size_t>(calibrationConfig.calibMinEmptySamples),
	        static_cast<std::size_t>(std::lround(calibrationConfig.calibMinEmptyFraction * static_cast<float>(boardSize) * static_cast<float>(boardSize))));

	float medianFinal = medianInitial;
	float sigmaFinal  = sigmaInitial;
	if (likelyEmptyDelta.size() >= minEmptyCount) {
		float medianLikely = 0.0f;
		float sigmaLikely  = 0.0f;
		if (robustMedianSigma(likelyEmptyDelta, calibrationConfig, medianLikely, sigmaLikely)) {
			medianFinal = medianLikely;
			sigmaFinal  = sigmaLikely;
		}
	}

	const std::vector<float>& chromaSource = likelyEmptyChroma.empty() ? allChroma : likelyEmptyChroma;
	float chromaMedian                     = calibrationConfig.chromaTFallback;
	if (!chromaSource.empty()) {
		std::vector<float> sortedChroma = chromaSource;
		std::sort(sortedChroma.begin(), sortedChroma.end());
		chromaMedian = medianSorted(sortedChroma);
	}

	outModel.medianEmpty = medianFinal;
	outModel.sigmaEmpty  = std::max(calibrationConfig.sigmaMin, sigmaFinal);
	outModel.tChromaSq   = std::max(calibrationConfig.chromaTMin, chromaMedian);
	return true;
}

} // namespace ModelCalibration

namespace Scoring {

static int edgeLevel(std::size_t index, int boardSize) {
	if (boardSize <= 0) {
		return 2;
	}
	const int gridX   = static_cast<int>(index) / boardSize;
	const int gridY   = static_cast<int>(index) - gridX * boardSize;
	const bool onEdge = (gridX == 0) || (gridX == boardSize - 1) || (gridY == 0) || (gridY == boardSize - 1);
	if (onEdge) {
		return 2;
	}
	const bool nearEdge = (gridX <= 1) || (gridX >= boardSize - 2) || (gridY <= 1) || (gridY >= boardSize - 2);
	return nearEdge ? 1 : 0;
}

static Scores computeScores(const Features& feature, const Model& model, const ScoringConfig& config) {
	Scores scores{};
	scores.z                 = (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	const float supportBlack = feature.darkFrac - feature.brightFrac;
	const float supportWhite = feature.brightFrac - feature.darkFrac;
	scores.chromaPenalty     = feature.chromaSq / (model.tChromaSq + feature.chromaSq);

	scores.black = config.scoreWDelta * (-scores.z) + config.scoreWSupport * supportBlack - config.scoreWChroma * scores.chromaPenalty;
	scores.white = config.scoreWDelta * (scores.z) + config.scoreWSupport * supportWhite - config.scoreWChroma * scores.chromaPenalty;
	scores.empty =
	        config.emptyScoreBias - config.emptyScoreZPenalty * std::abs(scores.z) - config.emptyScoreSupportPenalty * (feature.darkFrac + feature.brightFrac);
	return scores;
}

static Eval evaluate(const Features& feature, const Model& model, int edgeLevelValue, const ScoringConfig& config) {
	const Scores scores = computeScores(feature, model, config);
	Eval result{};
	result.z = scores.z;

	StoneState bestState = StoneState::Black;
	float bestScore      = scores.black;
	float secondScore    = scores.white;

	if (scores.white > bestScore) {
		bestState   = StoneState::White;
		bestScore   = scores.white;
		secondScore = scores.black;
	}

	if (scores.empty > bestScore) {
		bestState   = StoneState::Empty;
		bestScore   = scores.empty;
		secondScore = std::max(scores.black, scores.white);
	} else if (scores.empty > secondScore) {
		secondScore = scores.empty;
	}

	result.state       = bestState;
	result.bestScore   = bestScore;
	result.secondScore = secondScore;
	result.margin      = result.bestScore - result.secondScore;
	result.required    = config.margin0 * (1.0f + config.edgePenalty * static_cast<float>(edgeLevelValue));
	result.confidence  = std::clamp(result.margin / (result.required + 1e-6f), 0.0f, 1.0f);
	result.confidence *= std::clamp(1.0f - config.confChromaDownweight * scores.chromaPenalty, 0.0f, 1.0f);
	result.confidence = std::clamp(result.confidence, 0.0f, 1.0f);
	return result;
}

} // namespace Scoring

static float computeNeighborMedianDelta(const std::vector<Features>& features, int gridX, int gridY, int boardSize, float fallback) {
	std::array<float, 8> neighborValues{};
	int count = 0;
	for (int deltaX = -1; deltaX <= 1; ++deltaX) {
		for (int deltaY = -1; deltaY <= 1; ++deltaY) {
			if (deltaX == 0 && deltaY == 0) {
				continue;
			}
			const int neighborX = gridX + deltaX;
			const int neighborY = gridY + deltaY;
			if (neighborX < 0 || neighborX >= boardSize || neighborY < 0 || neighborY >= boardSize) {
				continue;
			}

			const std::size_t neighborIndex = static_cast<std::size_t>(neighborX) * static_cast<std::size_t>(boardSize) + static_cast<std::size_t>(neighborY);
			if (neighborIndex >= features.size() || !features[neighborIndex].valid) {
				continue;
			}

			neighborValues[static_cast<std::size_t>(count++)] = features[neighborIndex].deltaL;
		}
	}

	if (count == 0) {
		return fallback;
	}
	std::sort(neighborValues.begin(), neighborValues.begin() + count);
	return (count % 2 == 1) ? neighborValues[static_cast<std::size_t>(count / 2)]
	                        : 0.5f * (neighborValues[static_cast<std::size_t>(count / 2 - 1)] + neighborValues[static_cast<std::size_t>(count / 2)]);
}

static std::vector<float> computeNeighborMedianMap(const std::vector<Features>& features, int boardSize, float fallback) {
	std::vector<float> neighborMedianMap(features.size(), fallback);
	if (boardSize <= 0) {
		return neighborMedianMap;
	}
	for (std::size_t index = 0; index < features.size(); ++index) {
		const int gridX          = static_cast<int>(index) / boardSize;
		const int gridY          = static_cast<int>(index) - gridX * boardSize;
		neighborMedianMap[index] = computeNeighborMedianDelta(features, gridX, gridY, boardSize, fallback);
	}
	return neighborMedianMap;
}

static int computeRefinementExtent(double spacing, const RefinementConfig& config) {
	return roundedSpacingValue(spacing, config.refineExtentSpacingK, config.refineExtentFallback, config.refineExtentMin, config.refineExtentMax);
}

class DecisionPolicy {
public:
	enum class RefinementPath { None, EmptyRescue, Standard };

	explicit DecisionPolicy(const DecisionConfig& decisionConfig) : decisionConfig_(decisionConfig) {
	}

	Eval decide(const Features& feature, const SpatialContext& context, const Eval& evaluated, RejectionReason* outReason = nullptr) const {
		if (outReason != nullptr) {
			*outReason = RejectionReason::None;
		}
		RejectionReason rejectionReason{RejectionReason::None};
		if (!passesStatistical(evaluated, context, &rejectionReason) || !passesSupport(evaluated, feature, context, &rejectionReason) ||
		    !passesEdge(evaluated, feature, context, &rejectionReason) || !passesMargin(evaluated, &rejectionReason)) {
			if (outReason != nullptr) {
				*outReason = rejectionReason;
			}
			return rejected(evaluated);
		}
		return evaluated;
	}

	RefinementPath refinementPath(const Features& feature, const SpatialContext&, const Eval& evaluated) const {
		if (hasEmptyRescueHint(feature, evaluated)) {
			return RefinementPath::EmptyRescue;
		}
		if (hasStandardRefineHint(feature, evaluated)) {
			return RefinementPath::Standard;
		}
		return RefinementPath::None;
	}

	bool shouldRunRefinement(RefinementPath path, const Eval& evaluated) const {
		if (path == RefinementPath::None) {
			return false;
		}
		if (evaluated.state == StoneState::Empty) {
			return evaluated.margin < decisionConfig_.refineSkipStableEmptyMarginMult * evaluated.required;
		}
		return evaluated.margin < decisionConfig_.refineTriggerMult * evaluated.required;
	}

	bool acceptsRefinement(RefinementPath path, const Eval& baseEval, const Features& refinedFeature, const Eval& refinedEval) const {
		if (path == RefinementPath::None) {
			return false;
		}
		if (path == RefinementPath::Standard) {
			return refinedEval.margin > baseEval.margin + decisionConfig_.refineAcceptGainMult * baseEval.required;
		}
		const float minGain = decisionConfig_.refineAcceptFromEmptyGainMult * baseEval.required;
		return refinedEval.state == StoneState::White && refinedEval.margin > baseEval.margin + minGain &&
		       refinedEval.margin >= decisionConfig_.emptyRescueMinMarginMult * refinedEval.required &&
		       (refinedFeature.brightFrac - refinedFeature.darkFrac) >= decisionConfig_.minSupportAdvantageWhite;
	}

	bool passesStatistical(const Eval& evaluated, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		if (isWeakByZ(evaluated.state, evaluated.z, context.edgeLevel)) {
			return fail(outReason, RejectionReason::WeakZ);
		}
		if (evaluated.state == StoneState::Black && failsBlackConfidence(evaluated, context.boardSize)) {
			return fail(outReason, RejectionReason::LowConfidence);
		}
		if (evaluated.state == StoneState::White && failsWhiteConfidence(evaluated, context.boardSize)) {
			return fail(outReason, RejectionReason::LowConfidence);
		}
		return true;
	}

	bool passesSupport(const Eval& evaluated, const Features& feature, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		if (evaluated.state == StoneState::Black) {
			const bool weakSupport  = feature.darkFrac < decisionConfig_.minSupportBlack;
			const bool weakContrast = (feature.darkFrac - feature.brightFrac) < decisionConfig_.minSupportAdvantageBlack;
			const bool weakNeighbor = (context.neighborMedian - feature.deltaL) < decisionConfig_.minNeighborContrastBlack;
			if (weakSupport || weakContrast) {
				return fail(outReason, RejectionReason::WeakSupport);
			}
			if (weakNeighbor) {
				return fail(outReason, RejectionReason::WeakNeighborContrast);
			}
		}
		if (evaluated.state == StoneState::White && failsWhiteSupport(feature, context, evaluated.z)) {
			return fail(outReason, RejectionReason::WeakSupport);
		}
		return true;
	}

	bool passesEdge(const Eval& evaluated, const Features& feature, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		if (evaluated.state == StoneState::White && failsWhiteEdgeSanity(feature, context, evaluated.confidence)) {
			return fail(outReason, RejectionReason::EdgeArtifact);
		}
		return true;
	}

	bool passesMargin(const Eval& evaluated, RejectionReason* outReason = nullptr) const {
		if (evaluated.state == StoneState::Black && evaluated.margin < decisionConfig_.minBlackMarginMult * evaluated.required) {
			return fail(outReason, RejectionReason::MarginTooSmall);
		}
		if (evaluated.state == StoneState::White && evaluated.margin < decisionConfig_.minWhiteMarginMult * evaluated.required) {
			return fail(outReason, RejectionReason::MarginTooSmall);
		}
		return true;
	}

private:
	const DecisionConfig& decisionConfig_;

	static Eval rejected(const Eval& decision) {
		Eval rejectedDecision       = decision;
		rejectedDecision.state      = StoneState::Empty;
		rejectedDecision.confidence = 0.0f;
		return rejectedDecision;
	}

	static bool fail(RejectionReason* outReason, RejectionReason reason) {
		if (outReason != nullptr) {
			*outReason = reason;
		}
		return false;
	}

	bool isWeakByZ(StoneState state, float z, int edgeLevelValue) const {
		const float minBlackZ = decisionConfig_.minZBlack + (edgeLevelValue == 1 ? decisionConfig_.minZBlackNearEdgeAdd
		                                                                         : (edgeLevelValue == 2 ? decisionConfig_.minZBlackOnEdgeAdd : 0.0f));
		return (state == StoneState::Black && (-z) < minBlackZ) || (state == StoneState::White && z < decisionConfig_.minZWhite);
	}

	bool failsBlackConfidence(const Eval& decision, unsigned boardSize) const {
		const float threshold = (boardSize >= decisionConfig_.minConfidenceBlackBoardSize) ? decisionConfig_.minConfidenceBlack : 0.0f;
		return decision.confidence < threshold;
	}

	bool failsWhiteConfidence(const Eval& decision, unsigned boardSize) const {
		const float threshold = (boardSize >= decisionConfig_.minConfidenceWhiteBoardSize) ? decisionConfig_.minConfidenceWhite : 0.0f;
		return decision.confidence < threshold;
	}

	bool hasStrongWhiteSupport(const Features& feature, const SpatialContext& context) const {
		const float brightAdvantage  = feature.brightFrac - feature.darkFrac;
		const float neighborContrast = feature.deltaL - context.neighborMedian;
		return brightAdvantage >= decisionConfig_.whiteStrongAdvMin && neighborContrast >= decisionConfig_.whiteStrongNeighborMin;
	}

	bool qualifiesLowChromaRescue(const Features& feature, const SpatialContext& context, float z) const {
		const float chromaCap = (context.edgeLevel == 1) ? decisionConfig_.whiteLowChromaMaxNearEdge : decisionConfig_.whiteLowChromaMax;
		const float minBright = (context.edgeLevel == 1) ? decisionConfig_.whiteLowChromaMinBrightNearEdge : decisionConfig_.whiteLowChromaMinBright;
		if (feature.chromaSq > chromaCap || z < decisionConfig_.whiteLowChromaMinZ || feature.brightFrac < minBright) {
			return false;
		}
		constexpr float LOW_CHROMA_BAND_MAX = 90.0f;
		const bool inLowBand                = feature.chromaSq <= LOW_CHROMA_BAND_MAX;
		const bool inHighBand               = feature.chromaSq >= (0.75f * chromaCap);
		if (!inLowBand && !inHighBand) {
			return false;
		}
		if (context.edgeLevel == 1) {
			return inHighBand;
		}
		return true;
	}

	bool failsWhiteSupport(const Features& feature, const SpatialContext& context, float z) const {
		const float brightAdvantage = feature.brightFrac - feature.darkFrac;
		if (brightAdvantage < decisionConfig_.minSupportAdvantageWhiteFloor) {
			return true;
		}
		if (feature.brightFrac < decisionConfig_.minSupportWhite) {
			return true;
		}
		return !hasStrongWhiteSupport(feature, context) && !qualifiesLowChromaRescue(feature, context, z);
	}

	bool isNearEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return context.edgeLevel == 1 && feature.chromaSq >= decisionConfig_.edgeWhiteNearChromaSq &&
		       feature.brightFrac < decisionConfig_.edgeWhiteNearMinBrightFrac;
	}

	bool isOnEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return context.edgeLevel >= 2 && feature.chromaSq >= decisionConfig_.edgeWhiteHighChromaSq &&
		       feature.brightFrac < decisionConfig_.edgeWhiteMinBrightFrac;
	}

	bool isEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return isNearEdgeColorArtifact(feature, context) || isOnEdgeColorArtifact(feature, context);
	}

	bool isNearEdgeUnstableWhite(const Features& feature, const SpatialContext& context, float confidence) const {
		return context.edgeLevel == 1 && feature.chromaSq >= decisionConfig_.edgeWhiteNearWeakChromaSq &&
		       feature.brightFrac < decisionConfig_.edgeWhiteNearWeakBrightFrac && confidence < decisionConfig_.edgeWhiteNearWeakMinConf;
	}

	bool failsWhiteEdgeSanity(const Features& feature, const SpatialContext& context, float confidence) const {
		return isEdgeColorArtifact(feature, context) || isNearEdgeUnstableWhite(feature, context, confidence);
	}

	bool hasEmptyRescueHint(const Features& feature, const Eval& evaluated) const {
		if (evaluated.state != StoneState::Empty) {
			return false;
		}
		const float brightAdvantage = feature.brightFrac - feature.darkFrac;
		return evaluated.z >= decisionConfig_.emptyRescueMinZ && feature.brightFrac >= decisionConfig_.emptyRescueMinBright &&
		       brightAdvantage >= decisionConfig_.emptyRescueMinBrightAdv;
	}

	bool hasStandardRefineHint(const Features& feature, const Eval& evaluated) const {
		if (evaluated.state == StoneState::Empty) {
			return false;
		}
		const float baseSupportAdv = (evaluated.state == StoneState::Black) ? (feature.darkFrac - feature.brightFrac) : (feature.brightFrac - feature.darkFrac);
		const float minAbsZ        = (evaluated.state == StoneState::White) ? decisionConfig_.refineMinAbsZWhite : decisionConfig_.refineMinAbsZBlack;
		const float minSupportAdv =
		        (evaluated.state == StoneState::White) ? decisionConfig_.refineMinSupportAdvWhite : decisionConfig_.refineMinSupportAdvBlack;
		const bool allowed = std::abs(evaluated.z) >= minAbsZ && baseSupportAdv >= minSupportAdv;
		return allowed && evaluated.margin < decisionConfig_.refineTriggerMult * evaluated.required;
	}
};

class RefinementEngine {
public:
	RefinementEngine(const SampleContext& sampleContext, const Offsets& offsets, const Radii& radii, double spacing, const GeometryConfig& geometryConfig,
	                 const ScoringConfig& scoringConfig, const RefinementConfig& refinementConfig)
	    : sampleContext_(sampleContext), offsets_(offsets), radii_(radii), spacing_(spacing), geometryConfig_(geometryConfig), scoringConfig_(scoringConfig),
	      refinementConfig_(refinementConfig) {
	}

	bool searchBest(const cv::Point2f& intersection, const Model& model, const SpatialContext& context, const Features& baseFeature, const Eval& baseEval,
	                Features& outFeature, Eval& outEval) const {
		const int extent  = computeRefinementExtent(spacing_, refinementConfig_);
		const int centerX = static_cast<int>(std::lround(intersection.x));
		const int centerY = static_cast<int>(std::lround(intersection.y));

		bool sampledAny      = false;
		Eval bestEval        = baseEval;
		Features bestFeature = baseFeature;
		for (int offsetY = -extent; offsetY <= extent; offsetY += refinementConfig_.refineStepPx) {
			for (int offsetX = -extent; offsetX <= extent; offsetX += refinementConfig_.refineStepPx) {
				if (offsetX == 0 && offsetY == 0) {
					continue;
				}
				Features candidateFeature{};
				if (!FeatureExtraction::computeFeaturesAt(sampleContext_, offsets_, radii_, geometryConfig_, centerX + offsetX, centerY + offsetY,
				                                          candidateFeature)) {
					continue;
				}
				sampledAny               = true;
				const Eval candidateEval = Scoring::evaluate(candidateFeature, model, context.edgeLevel, scoringConfig_);
				if (isBetterCandidate(bestEval, candidateEval, refinementConfig_)) {
					bestEval    = candidateEval;
					bestFeature = candidateFeature;
				}
			}
		}
		outFeature = bestFeature;
		outEval    = bestEval;
		return sampledAny;
	}

private:
	const SampleContext& sampleContext_;
	const Offsets& offsets_;
	const Radii& radii_;
	double spacing_{0.0};
	const GeometryConfig& geometryConfig_;
	const ScoringConfig& scoringConfig_;
	const RefinementConfig& refinementConfig_;

	static bool isBetterCandidate(const Eval& currentBest, const Eval& candidate, const RefinementConfig& refinementConfig) {
		const bool betterMargin      = candidate.margin > currentBest.margin;
		const bool promotesFromEmpty = (currentBest.state == StoneState::Empty) && (candidate.state != StoneState::Empty) &&
		                               (candidate.margin + refinementConfig.refinePromoteFromEmptyEps >= currentBest.margin);
		return betterMargin || promotesFromEmpty;
	}
};

namespace Debugging {

static bool isRuntimeDebugEnabled() {
	const char* debugEnv = std::getenv("GO_STONE_DEBUG");
	if (debugEnv == nullptr) {
		return false;
	}
	const std::string_view debugFlag(debugEnv);
	return debugFlag == "1" || debugFlag == "2";
}

static const char* rejectionReasonLabel(RejectionReason reason) {
	switch (reason) {
	case RejectionReason::None:
		return "None";
	case RejectionReason::WeakZ:
		return "WeakZ";
	case RejectionReason::LowConfidence:
		return "LowConfidence";
	case RejectionReason::WeakSupport:
		return "WeakSupport";
	case RejectionReason::WeakNeighborContrast:
		return "WeakNeighborContrast";
	case RejectionReason::EdgeArtifact:
		return "EdgeArtifact";
	case RejectionReason::MarginTooSmall:
		return "MarginTooSmall";
	case RejectionReason::Other:
		return "Other";
	}
	return "Other";
}

static cv::Mat drawOverlay(const cv::Mat& image, const std::vector<cv::Point2f>& intersections, const std::vector<StoneState>& states, int radius) {
	cv::Mat overlay = image.clone();
	for (std::size_t index = 0; index < intersections.size() && index < states.size(); ++index) {
		if (states[index] == StoneState::Black) {
			cv::circle(overlay, intersections[index], radius, cv::Scalar(0, 0, 0), 2);
		} else if (states[index] == StoneState::White) {
			cv::circle(overlay, intersections[index], radius, cv::Scalar(255, 0, 0), 2);
		}
	}
	return overlay;
}

static cv::Mat renderStatsTile(const Model& model, const DebugStats& stats) {
	cv::Mat tile(220, 450, CV_8UC3, cv::Scalar(255, 255, 255));
	int y              = 24;
	const auto putLine = [&](const std::string& line) {
		cv::putText(tile, line, cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
		y += 22;
	};

	putLine("Stone Detection v2");
	putLine(std::string(cv::format("medianEmpty: %.2f", model.medianEmpty)));
	putLine(std::string(cv::format("sigmaEmpty: %.2f", model.sigmaEmpty)));
	putLine(std::string(cv::format("chromaT: %.1f", model.tChromaSq)));
	putLine("black: " + std::to_string(stats.blackCount));
	putLine("white: " + std::to_string(stats.whiteCount));
	putLine("empty: " + std::to_string(stats.emptyCount));
	putLine("refine tried: " + std::to_string(stats.refinedTried));
	putLine("refine accepted: " + std::to_string(stats.refinedAccepted));
	return tile;
}

static void emitRuntimeDebug(const BoardGeometry& geometry, const std::vector<Features>& features, const Model& model, const std::vector<StoneState>& states,
                             const std::vector<float>& confidence, const std::vector<Eval>& evaluations, const std::vector<float>& neighborMedianMap,
                             const DebugStats& stats, const std::vector<RejectionReason>* rejectionReasons = nullptr) {
	const char* debugEnv = std::getenv("GO_STONE_DEBUG");
	if (debugEnv == nullptr) {
		return;
	}

	const std::string_view debugFlag(debugEnv);
	if (debugFlag != "1" && debugFlag != "2") {
		return;
	}

	const int boardSize        = static_cast<int>(geometry.boardSize);
	const bool hasEvaluations  = evaluations.size() == states.size();
	const bool hasNeighborMeds = neighborMedianMap.size() == states.size();
	const auto zForIndex       = [&](std::size_t index, const Features& feature) {
        return hasEvaluations ? evaluations[index].z : (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	};
	const auto rawStateForIndex = [&](std::size_t index) {
		if (!hasEvaluations) {
			return StoneState::Empty;
		}
		return evaluations[index].state;
	};
	const auto neighborForIndex = [&](std::size_t index) {
		if (hasNeighborMeds) {
			return neighborMedianMap[index];
		}
		const int gridX = static_cast<int>(index) / boardSize;
		const int gridY = static_cast<int>(index) - gridX * boardSize;
		return computeNeighborMedianDelta(features, gridX, gridY, boardSize, model.medianEmpty);
	};

	std::cerr << "[stone-debug] N=" << geometry.boardSize << " black=" << stats.blackCount << " white=" << stats.whiteCount << " empty=" << stats.emptyCount
	          << " median=" << model.medianEmpty << " sigma=" << model.sigmaEmpty << " chromaT=" << model.tChromaSq << " refineTried=" << stats.refinedTried
	          << " refineAccepted=" << stats.refinedAccepted << '\n';

	for (std::size_t index = 0; index < states.size(); ++index) {
		if (states[index] == StoneState::Empty) {
			continue;
		}
		const std::size_t gridX      = index / geometry.boardSize;
		const std::size_t gridY      = index % geometry.boardSize;
		const Features& feature      = features[index];
		const float z                = zForIndex(index, feature);
		const float neighborMedian   = neighborForIndex(index);
		const float neighborContrast = (states[index] == StoneState::Black) ? (neighborMedian - feature.deltaL) : (feature.deltaL - neighborMedian);
		const cv::Point2f point      = geometry.intersections[index];
		std::cerr << "  idx=" << index << " (" << gridX << "," << gridY << ")"
		          << " px=(" << point.x << "," << point.y << ")"
		          << " state=" << (states[index] == StoneState::Black ? "B" : "W") << " conf=" << confidence[index] << " z=" << z << " d=" << feature.darkFrac
		          << " b=" << feature.brightFrac << " c=" << feature.chromaSq << " nc=" << neighborContrast << '\n';
	}

	const bool verboseCandidates = (debugFlag == "2");
	if (verboseCandidates) {
		struct EmptyRow {
			std::size_t idx{0};
			float z{0.0f};
		};
		std::vector<EmptyRow> emptyRows;
		emptyRows.reserve(features.size());
		for (std::size_t index = 0; index < features.size(); ++index) {
			if (!features[index].valid || states[index] != StoneState::Empty) {
				continue;
			}
			const float z = zForIndex(index, features[index]);
			emptyRows.push_back({index, z});
		}
		std::sort(emptyRows.begin(), emptyRows.end(), [](const EmptyRow& left, const EmptyRow& right) { return left.z > right.z; });
		const std::size_t limit = std::min<std::size_t>(20, emptyRows.size());
		for (std::size_t row = 0; row < limit; ++row) {
			const std::size_t index      = emptyRows[row].idx;
			const std::size_t gridX      = index / geometry.boardSize;
			const std::size_t gridY      = index % geometry.boardSize;
			const StoneState rawState    = rawStateForIndex(index);
			const float rawMargin        = hasEvaluations ? evaluations[index].margin : 0.0f;
			const float rawRequired      = hasEvaluations ? evaluations[index].required : 0.0f;
			const float rawConf          = hasEvaluations ? evaluations[index].confidence : 0.0f;
			const float neighborMedian   = neighborForIndex(index);
			const float neighborContrast = features[index].deltaL - neighborMedian;
			std::cerr << "  empty-cand idx=" << index << " (" << gridX << "," << gridY << ")"
			          << " z=" << emptyRows[row].z << " d=" << features[index].darkFrac << " b=" << features[index].brightFrac
			          << " c=" << features[index].chromaSq << " raw=" << (rawState == StoneState::Black ? "B" : (rawState == StoneState::White ? "W" : "E"))
			          << " m=" << rawMargin << "/" << rawRequired << " conf=" << rawConf << " nc=" << neighborContrast << '\n';
		}

		struct BrightRow {
			std::size_t idx{0};
			float bright{0.0f};
		};
		std::vector<BrightRow> brightRows;
		brightRows.reserve(features.size());
		for (std::size_t index = 0; index < features.size(); ++index) {
			if (!features[index].valid || states[index] != StoneState::Empty) {
				continue;
			}
			brightRows.push_back({index, features[index].brightFrac});
		}
		std::sort(brightRows.begin(), brightRows.end(), [](const BrightRow& left, const BrightRow& right) { return left.bright > right.bright; });
		const std::size_t brightLimit = std::min<std::size_t>(20, brightRows.size());
		for (std::size_t row = 0; row < brightLimit; ++row) {
			const std::size_t index      = brightRows[row].idx;
			const std::size_t gridX      = index / geometry.boardSize;
			const std::size_t gridY      = index % geometry.boardSize;
			const float z                = zForIndex(index, features[index]);
			const StoneState rawState    = rawStateForIndex(index);
			const float rawMargin        = hasEvaluations ? evaluations[index].margin : 0.0f;
			const float rawRequired      = hasEvaluations ? evaluations[index].required : 0.0f;
			const float rawConf          = hasEvaluations ? evaluations[index].confidence : 0.0f;
			const float neighborMedian   = neighborForIndex(index);
			const float neighborContrast = features[index].deltaL - neighborMedian;
			std::cerr << "  bright-cand idx=" << index << " (" << gridX << "," << gridY << ")"
			          << " b=" << brightRows[row].bright << " z=" << z << " d=" << features[index].darkFrac << " c=" << features[index].chromaSq
			          << " raw=" << (rawState == StoneState::Black ? "B" : (rawState == StoneState::White ? "W" : "E")) << " m=" << rawMargin << "/"
			          << rawRequired << " conf=" << rawConf << " nc=" << neighborContrast << '\n';
		}
	}

	if (stats.blackCount + stats.whiteCount == 0) {
		struct CandidateRow {
			std::size_t idx{0};
			float absZ{0.0f};
		};
		std::vector<CandidateRow> rows;
		rows.reserve(features.size());
		for (std::size_t index = 0; index < features.size(); ++index) {
			if (!features[index].valid) {
				continue;
			}
			const float z = zForIndex(index, features[index]);
			rows.push_back({index, std::abs(z)});
		}
		std::sort(rows.begin(), rows.end(), [](const CandidateRow& left, const CandidateRow& right) { return left.absZ > right.absZ; });
		const std::size_t limit = std::min<std::size_t>(10, rows.size());
		for (std::size_t row = 0; row < limit; ++row) {
			const std::size_t index = rows[row].idx;
			const std::size_t gridX = index / geometry.boardSize;
			const std::size_t gridY = index % geometry.boardSize;
			const Features& feature = features[index];
			const float z           = zForIndex(index, feature);
			std::cerr << "  cand idx=" << index << " (" << gridX << "," << gridY << ")"
			          << " z=" << z << " d=" << feature.darkFrac << " b=" << feature.brightFrac << " c=" << feature.chromaSq << '\n';
		}
	}

	if (rejectionReasons != nullptr && rejectionReasons->size() == states.size()) {
		std::array<int, 8> reasonCounts{};
		for (std::size_t index = 0; index < states.size(); ++index) {
			if (states[index] != StoneState::Empty) {
				continue;
			}
			const std::size_t reasonIndex = static_cast<std::size_t>((*rejectionReasons)[index]);
			if (reasonIndex < reasonCounts.size()) {
				++reasonCounts[reasonIndex];
			}
		}
		std::cerr << "[stone-debug] rejections";
		for (std::size_t index = 0; index < reasonCounts.size(); ++index) {
			std::cerr << " " << rejectionReasonLabel(static_cast<RejectionReason>(index)) << "=" << reasonCounts[index];
		}
		std::cerr << '\n';
	}
}

} // namespace Debugging

static void classifyAll(const std::vector<cv::Point2f>& intersections, const std::vector<Features>& features, const Model& model, unsigned boardSize,
                        const ScoringConfig& scoringConfig, const DecisionConfig& decisionConfig, const RefinementConfig& refinementConfig,
                        const RefinementEngine& refinementEngine, std::vector<StoneState>& outStates, std::vector<float>& outConfidence, DebugStats& outStats,
                        std::vector<Eval>* outEvaluations = nullptr, std::vector<float>* outNeighborMedianMap = nullptr,
                        std::vector<RejectionReason>* outRejectionReasons = nullptr) {
	outStates.assign(intersections.size(), StoneState::Empty);
	outConfidence.assign(intersections.size(), 0.0f);
	outStats = DebugStats{};
	(void)refinementConfig;
	if (outEvaluations != nullptr) {
		outEvaluations->assign(intersections.size(), Eval{});
	}
	if (outRejectionReasons != nullptr) {
		outRejectionReasons->assign(intersections.size(), RejectionReason::None);
	}

	const int boardSizeInt                     = static_cast<int>(boardSize);
	const std::vector<float> neighborMedianMap = computeNeighborMedianMap(features, boardSizeInt, model.medianEmpty);
	if (outNeighborMedianMap != nullptr) {
		*outNeighborMedianMap = neighborMedianMap;
	}
	const DecisionPolicy policy(decisionConfig);

	for (std::size_t index = 0; index < intersections.size(); ++index) {
		if (!features[index].valid) {
			if (outRejectionReasons != nullptr) {
				(*outRejectionReasons)[index] = RejectionReason::Other;
			}
			++outStats.emptyCount;
			continue;
		}

		Features feature = features[index];
		const SpatialContext context{Scoring::edgeLevel(index, boardSizeInt), neighborMedianMap[index], boardSize};

		const Eval baseEval = Scoring::evaluate(feature, model, context.edgeLevel, scoringConfig);
		RejectionReason rejectionReason{RejectionReason::None};
		const Eval baseDecision = policy.decide(feature, context, baseEval, outRejectionReasons != nullptr ? &rejectionReason : nullptr);
		Eval decision           = baseDecision;

		const DecisionPolicy::RefinementPath path = policy.refinementPath(feature, context, baseEval);
		if (path != DecisionPolicy::RefinementPath::None) {
			++outStats.refinedTried;
			if (policy.shouldRunRefinement(path, baseEval)) {
				Features refinedFeature = feature;
				Eval bestRawEval{};
				const bool refined = refinementEngine.searchBest(intersections[index], model, context, feature, baseEval, refinedFeature, bestRawEval);
				if (refined && policy.acceptsRefinement(path, baseEval, refinedFeature, bestRawEval)) {
					feature  = refinedFeature;
					decision = policy.decide(feature, context, bestRawEval, outRejectionReasons != nullptr ? &rejectionReason : nullptr);
					++outStats.refinedAccepted;
				}
			}
		}

		outStates[index]     = decision.state;
		outConfidence[index] = decision.confidence;
		if (outEvaluations != nullptr) {
			(*outEvaluations)[index] = baseEval;
		}
		if (outRejectionReasons != nullptr) {
			(*outRejectionReasons)[index] = (decision.state == StoneState::Empty) ? rejectionReason : RejectionReason::None;
		}
		if (decision.state == StoneState::Black) {
			++outStats.blackCount;
		} else if (decision.state == StoneState::White) {
			++outStats.whiteCount;
		} else {
			++outStats.emptyCount;
		}
	}
}

} // namespace

StoneResult analyseBoardV2(const BoardGeometry& geometry, DebugVisualizer* debugger, const StoneDetectionConfig& config) {
	if (geometry.image.empty()) {
		std::cerr << "Stone detection failed: input image is empty\n";
		return {false, {}, {}};
	}
	if (geometry.boardSize == 0u || geometry.intersections.size() != geometry.boardSize * geometry.boardSize) {
		std::cerr << "Stone detection failed: invalid board geometry\n";
		return {false, {}, {}};
	}

	if (debugger) {
		debugger->beginStage("Stone Detection v2");
		debugger->add("Input", geometry.image);
	}

	const Radii radii     = GeometrySampling::chooseRadii(geometry.spacing, config.geometry);
	const Offsets offsets = GeometrySampling::precomputeOffsets(radii);

	LabBlur blurredLab{};
	if (!FeatureExtraction::prepareLabBlur(geometry.image, radii, config.geometry, blurredLab)) {
		if (debugger) {
			debugger->endStage();
		}
		std::cerr << "Stone detection failed: unsupported channel count\n";
		return {false, {}, {}};
	}
	const SampleContext sampleContext{blurredLab.L, blurredLab.A, blurredLab.B, blurredLab.L.rows, blurredLab.L.cols};

	const std::vector<Features> features = FeatureExtraction::computeFeatures(geometry.intersections, sampleContext, offsets, radii, config.geometry);
	const RefinementEngine refinementEngine(sampleContext, offsets, radii, geometry.spacing, config.geometry, config.scoring, config.refinement);

	Model model{};
	if (!ModelCalibration::calibrateModel(features, geometry.boardSize, config.calibration, model)) {
		if (debugger) {
			debugger->endStage();
		}
		std::cerr << "Stone detection failed: calibration failed\n";
		return {false, {}, {}};
	}

	// Refactor-only: Behavior preserved. No functional changes.
	std::vector<StoneState> states;
	std::vector<float> confidence;
	DebugStats stats{};
	std::vector<Eval> evaluations;
	std::vector<float> neighborMedianMap;
	std::vector<RejectionReason> rejectionReasons;
	const bool collectRuntimeDebug = Debugging::isRuntimeDebugEnabled();
	classifyAll(geometry.intersections, features, model, geometry.boardSize, config.scoring, config.decision, config.refinement, refinementEngine, states,
	            confidence, stats, collectRuntimeDebug ? &evaluations : nullptr, collectRuntimeDebug ? &neighborMedianMap : nullptr,
	            collectRuntimeDebug ? &rejectionReasons : nullptr);

	Debugging::emitRuntimeDebug(geometry, features, model, states, confidence, evaluations, neighborMedianMap, stats,
	                            collectRuntimeDebug ? &rejectionReasons : nullptr);

	if (debugger) {
		debugger->add("Stone Overlay", Debugging::drawOverlay(geometry.image, geometry.intersections, states, radii.innerRadius));
		debugger->add("Stone Stats", Debugging::renderStatsTile(model, stats));
		debugger->endStage();
	}

	return {true, std::move(states), std::move(confidence)};
}

StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger, const StoneDetectionConfig& config) {
	return analyseBoardV2(geometry, debugger, config);
}

} // namespace go::vision::core
