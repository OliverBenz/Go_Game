#include "camera/stoneFinder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <opencv2/opencv.hpp>

namespace go::camera {

namespace {

struct StoneDetectionConfig {
	int innerRadiusFallback{6};
	double innerRadiusSpacingK{0.24};
	int innerRadiusMin{2};
	int innerRadiusMax{30};

	int bgRadiusMin{2};
	int bgRadiusMax{12};

	double bgOffsetSpacingK{0.48};
	int bgOffsetMinExtra{2};
	int bgOffsetFallbackAdd{6};
	int minBgSamples{5};

	float supportDelta{18.0f};

	float labNeutral{128.0f};
	double blurSigmaRadiusK{0.15};
	double blurSigmaMin{1.0};
	double blurSigmaMax{4.0};

	float madToSigma{1.4826f};
	float sigmaMin{5.0f};
	float emptyBandSigma{1.80f};
	float likelyEmptySupportSumMax{0.35f};
	int calibMinEmptySamples{8};
	float calibMinEmptyFraction{0.10f};
	float chromaTFallback{400.0f};
	float chromaTMin{100.0f};

	float scoreWDelta{1.0f};
	float scoreWSupport{0.2f};
	float scoreWChroma{0.9f};
	float margin0{1.5f};
	float edgePenalty{0.20f};
	float confChromaDownweight{0.25f};
	float emptyScoreBias{0.30f};
	float emptyScoreZPenalty{0.75f};
	float emptyScoreSupportPenalty{0.15f};

	float minZBlack{3.8f};
	float minZBlackNearEdgeAdd{0.4f};
	float minZBlackOnEdgeAdd{1.2f};
	float minZWhite{0.6f};

	float minSupportBlack{0.50f};
	float minSupportWhite{0.08f};
	float minSupportAdvantageBlack{0.08f};
	float minSupportAdvantageWhite{0.03f};

	float minNeighborContrastBlack{14.0f};
	float minNeighborContrastWhite{0.0f};

	float whiteStrongAdvMin{0.08f};
	float whiteStrongNeighborMin{12.0f};
	float whiteLowChromaMax{55.0f};
	float whiteLowChromaMaxNearEdge{35.0f};
	float whiteLowChromaMinZ{1.0f};
	float whiteLowChromaMinBright{0.10f};
	float whiteLowChromaMinBrightNearEdge{0.16f};

	float edgeWhiteNearChromaSq{70.0f};
	float edgeWhiteNearMinBrightFrac{0.22f};
	float edgeWhiteNearWeakChromaSq{45.0f};
	float edgeWhiteNearWeakBrightFrac{0.22f};
	float edgeWhiteNearWeakMinConf{0.965f};
	float edgeWhiteHighChromaSq{120.0f};
	float edgeWhiteMinBrightFrac{0.35f};

	float minConfidenceBlack{0.90f};
	unsigned minConfidenceBlackBoardSize{13u};

	float minBlackMarginMult{1.0f};
	float minWhiteMarginMult{0.30f};

	float refineTriggerMult{1.25f};
	double refineExtentSpacingK{0.09};
	int refineExtentFallback{6};
	int refineExtentMin{4};
	int refineExtentMax{8};
	int refineStepPx{2};

	float refineSkipStableEmptyMarginMult{0.80f};
	float refineAcceptGainMult{0.20f};
	float refineAcceptFromEmptyGainMult{0.10f};
	float refinePromoteFromEmptyEps{1e-4f};

	float emptyRescueMinZ{0.35f};
	float emptyRescueMinBright{0.08f};
	float emptyRescueMinBrightAdv{0.02f};
	float emptyRescueMinMarginMult{0.35f};

	float refineMinAbsZWhite{1.2f};
	float refineMinAbsZBlack{2.0f};
	float refineMinSupportAdvWhite{0.20f};
	float refineMinSupportAdvBlack{0.35f};
};

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
	float wDelta{0.0f};
	float wSupport{0.0f};
	float wChroma{0.0f};
	float tChromaSq{0.0f};
	float margin0{0.0f};
	float edgePenalty{0.0f};
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

static Radii chooseRadii(double spacing, const StoneDetectionConfig& config) {
	Radii radii{};
	int innerRadius = config.innerRadiusFallback;
	if (std::isfinite(spacing) && spacing > 0.0) {
		innerRadius = static_cast<int>(std::lround(spacing * config.innerRadiusSpacingK));
	}

	radii.innerRadius = std::clamp(innerRadius, config.innerRadiusMin, config.innerRadiusMax);
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

static bool prepareLabBlur(const cv::Mat& image, const Radii& radii, const StoneDetectionConfig& config, LabBlur& outBlur) {
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

static bool computeFeaturesAt(const SampleContext& context, const Offsets& offsets, const Radii& radii, const StoneDetectionConfig& config, int centerX,
                              int centerY, Features& outFeatures) {
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
                                             const Radii& radii, const StoneDetectionConfig& config) {
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

static bool robustMedianSigma(const std::vector<float>& values, const StoneDetectionConfig& config, float& outMedian, float& outSigma) {
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

static bool calibrateModel(const std::vector<Features>& features, unsigned boardSize, const StoneDetectionConfig& config, Model& outModel) {
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
	float sigmaInitial  = config.sigmaMin;
	if (!robustMedianSigma(allDelta, config, medianInitial, sigmaInitial)) {
		return false;
	}

	const float emptyBand = config.emptyBandSigma * sigmaInitial;
	std::vector<float> likelyEmptyDelta;
	std::vector<float> likelyEmptyChroma;
	likelyEmptyDelta.reserve(allDelta.size());
	likelyEmptyChroma.reserve(allChroma.size());
	for (const Features& feature: features) {
		if (!feature.valid) {
			continue;
		}
		const bool inBand     = std::abs(feature.deltaL - medianInitial) <= emptyBand;
		const bool lowSupport = (feature.darkFrac + feature.brightFrac) <= config.likelyEmptySupportSumMax;
		if (inBand && lowSupport) {
			likelyEmptyDelta.push_back(feature.deltaL);
			likelyEmptyChroma.push_back(feature.chromaSq);
		}
	}

	const std::size_t minEmptyCount = std::max<std::size_t>(
	        static_cast<std::size_t>(config.calibMinEmptySamples),
	        static_cast<std::size_t>(std::lround(config.calibMinEmptyFraction * static_cast<float>(boardSize) * static_cast<float>(boardSize))));

	float medianFinal = medianInitial;
	float sigmaFinal  = sigmaInitial;
	if (likelyEmptyDelta.size() >= minEmptyCount) {
		float medianLikely = 0.0f;
		float sigmaLikely  = 0.0f;
		if (robustMedianSigma(likelyEmptyDelta, config, medianLikely, sigmaLikely)) {
			medianFinal = medianLikely;
			sigmaFinal  = sigmaLikely;
		}
	}

	const std::vector<float>& chromaSource = likelyEmptyChroma.empty() ? allChroma : likelyEmptyChroma;
	float chromaMedian                     = config.chromaTFallback;
	if (!chromaSource.empty()) {
		std::vector<float> sortedChroma = chromaSource;
		std::sort(sortedChroma.begin(), sortedChroma.end());
		chromaMedian = medianSorted(sortedChroma);
	}

	outModel.medianEmpty = medianFinal;
	outModel.sigmaEmpty  = std::max(config.sigmaMin, sigmaFinal);
	outModel.tChromaSq   = std::max(config.chromaTMin, chromaMedian);
	outModel.wDelta      = config.scoreWDelta;
	outModel.wSupport    = config.scoreWSupport;
	outModel.wChroma     = config.scoreWChroma;
	outModel.margin0     = config.margin0;
	outModel.edgePenalty = config.edgePenalty;
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

static Scores computeScores(const Features& feature, const Model& model, const StoneDetectionConfig& config) {
	Scores scores{};
	scores.z                 = (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	const float supportBlack = feature.darkFrac - feature.brightFrac;
	const float supportWhite = feature.brightFrac - feature.darkFrac;
	scores.chromaPenalty     = feature.chromaSq / (model.tChromaSq + feature.chromaSq);

	scores.black = model.wDelta * (-scores.z) + model.wSupport * supportBlack - model.wChroma * scores.chromaPenalty;
	scores.white = model.wDelta * (scores.z) + model.wSupport * supportWhite - model.wChroma * scores.chromaPenalty;
	scores.empty =
	        config.emptyScoreBias - config.emptyScoreZPenalty * std::abs(scores.z) - config.emptyScoreSupportPenalty * (feature.darkFrac + feature.brightFrac);
	return scores;
}

static Eval evaluate(const Features& feature, const Model& model, int edgeLevelValue, const StoneDetectionConfig& config) {
	const Scores scores = computeScores(feature, model, config);
	std::array<std::pair<StoneState, float>, 3> ranked{
	        std::pair<StoneState, float>{StoneState::Black, scores.black},
	        std::pair<StoneState, float>{StoneState::White, scores.white},
	        std::pair<StoneState, float>{StoneState::Empty, scores.empty},
	};
	std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) { return left.second > right.second; });

	Eval result{};
	result.state       = ranked[0].first;
	result.bestScore   = ranked[0].second;
	result.secondScore = ranked[1].second;
	result.margin      = result.bestScore - result.secondScore;
	result.required    = model.margin0 * (1.0f + model.edgePenalty * static_cast<float>(edgeLevelValue));
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

static int computeRefinementExtent(double spacing, const StoneDetectionConfig& config) {
	int extent = config.refineExtentFallback;
	if (std::isfinite(spacing) && spacing > 0.0) {
		extent = static_cast<int>(std::lround(spacing * config.refineExtentSpacingK));
	}
	return std::clamp(extent, config.refineExtentMin, config.refineExtentMax);
}

class DecisionPolicy {
public:
	enum class RefinementPath { None, EmptyRescue, Standard };

	explicit DecisionPolicy(const StoneDetectionConfig& config) : config_(config) {
	}

	Eval decide(const Features& feature, const Model& model, const SpatialContext& context, const Eval& evaluated, RejectionReason* outReason = nullptr) const {
		if (outReason != nullptr) {
			*outReason = RejectionReason::None;
		}
		const float z = normalizedDelta(feature, model);
		RejectionReason rejectionReason{RejectionReason::None};
		if (!passesStatistical(evaluated, z, context, &rejectionReason) || !passesSupport(evaluated, feature, z, context, &rejectionReason) ||
		    !passesEdge(evaluated, feature, context, &rejectionReason) || !passesMargin(evaluated, &rejectionReason)) {
			if (outReason != nullptr) {
				*outReason = rejectionReason;
			}
			return rejected(evaluated);
		}
		return evaluated;
	}

	Eval decide(const Features& feature, const Model& model, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		return decide(feature, model, context, Scoring::evaluate(feature, model, context.edgeLevel, config_), outReason);
	}

	RefinementPath refinementPath(const Features& feature, const Model& model, const SpatialContext&, const Eval& evaluated) const {
		if (hasEmptyRescueHint(feature, model, evaluated)) {
			return RefinementPath::EmptyRescue;
		}
		if (hasStandardRefineHint(feature, model, evaluated)) {
			return RefinementPath::Standard;
		}
		return RefinementPath::None;
	}

	bool shouldRunRefinement(RefinementPath path, const Eval& evaluated) const {
		if (path == RefinementPath::None) {
			return false;
		}
		if (evaluated.state == StoneState::Empty) {
			return evaluated.margin < config_.refineSkipStableEmptyMarginMult * evaluated.required;
		}
		return evaluated.margin < config_.refineTriggerMult * evaluated.required;
	}

	bool acceptsRefinement(RefinementPath path, const Eval& baseEval, const Features& refinedFeature, const Eval& refinedEval) const {
		if (path == RefinementPath::None) {
			return false;
		}
		if (path == RefinementPath::Standard) {
			return refinedEval.margin > baseEval.margin + config_.refineAcceptGainMult * baseEval.required;
		}
		const float minGain = config_.refineAcceptFromEmptyGainMult * baseEval.required;
		return refinedEval.state == StoneState::White && refinedEval.margin > baseEval.margin + minGain &&
		       refinedEval.margin >= config_.emptyRescueMinMarginMult * refinedEval.required &&
		       (refinedFeature.brightFrac - refinedFeature.darkFrac) >= config_.minSupportAdvantageWhite;
	}

	bool passesStatistical(const Eval& evaluated, float z, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		if (isWeakByZ(evaluated.state, z, context.edgeLevel)) {
			return fail(outReason, RejectionReason::WeakZ);
		}
		if (evaluated.state == StoneState::Black && failsBlackConfidence(evaluated, context.boardSize)) {
			return fail(outReason, RejectionReason::LowConfidence);
		}
		return true;
	}

	bool passesSupport(const Eval& evaluated, const Features& feature, float z, const SpatialContext& context, RejectionReason* outReason = nullptr) const {
		if (evaluated.state == StoneState::Black) {
			const bool weakSupport  = feature.darkFrac < config_.minSupportBlack;
			const bool weakContrast = (feature.darkFrac - feature.brightFrac) < config_.minSupportAdvantageBlack;
			const bool weakNeighbor = (context.neighborMedian - feature.deltaL) < config_.minNeighborContrastBlack;
			if (weakSupport || weakContrast) {
				return fail(outReason, RejectionReason::WeakSupport);
			}
			if (weakNeighbor) {
				return fail(outReason, RejectionReason::WeakNeighborContrast);
			}
		}
		if (evaluated.state == StoneState::White && failsWhiteSupport(feature, context, z)) {
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
		if (evaluated.state == StoneState::Black && evaluated.margin < config_.minBlackMarginMult * evaluated.required) {
			return fail(outReason, RejectionReason::MarginTooSmall);
		}
		if (evaluated.state == StoneState::White && evaluated.margin < config_.minWhiteMarginMult * evaluated.required) {
			return fail(outReason, RejectionReason::MarginTooSmall);
		}
		return true;
	}

private:
	const StoneDetectionConfig& config_;

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

	static float normalizedDelta(const Features& feature, const Model& model) {
		return (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	}

	bool isWeakByZ(StoneState state, float z, int edgeLevelValue) const {
		const float minBlackZ =
		        config_.minZBlack + (edgeLevelValue == 1 ? config_.minZBlackNearEdgeAdd : (edgeLevelValue == 2 ? config_.minZBlackOnEdgeAdd : 0.0f));
		return (state == StoneState::Black && (-z) < minBlackZ) || (state == StoneState::White && z < config_.minZWhite);
	}

	bool failsBlackConfidence(const Eval& decision, unsigned boardSize) const {
		const float threshold = (boardSize >= config_.minConfidenceBlackBoardSize) ? config_.minConfidenceBlack : 0.0f;
		return decision.confidence < threshold;
	}

	bool hasStrongWhiteSupport(const Features& feature, const SpatialContext& context) const {
		const float brightAdvantage  = feature.brightFrac - feature.darkFrac;
		const float neighborContrast = feature.deltaL - context.neighborMedian;
		return brightAdvantage >= config_.whiteStrongAdvMin && neighborContrast >= config_.whiteStrongNeighborMin;
	}

	bool qualifiesLowChromaRescue(const Features& feature, const SpatialContext& context, float z) const {
		const float chromaCap = (context.edgeLevel == 1) ? config_.whiteLowChromaMaxNearEdge : config_.whiteLowChromaMax;
		const float minBright = (context.edgeLevel == 1) ? config_.whiteLowChromaMinBrightNearEdge : config_.whiteLowChromaMinBright;
		return feature.chromaSq <= chromaCap && z >= config_.whiteLowChromaMinZ && feature.brightFrac >= minBright;
	}

	bool failsWhiteSupport(const Features& feature, const SpatialContext& context, float z) const {
		if (feature.brightFrac < config_.minSupportWhite) {
			return true;
		}
		return !hasStrongWhiteSupport(feature, context) && !qualifiesLowChromaRescue(feature, context, z);
	}

	bool isNearEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return context.edgeLevel == 1 && feature.chromaSq >= config_.edgeWhiteNearChromaSq && feature.brightFrac < config_.edgeWhiteNearMinBrightFrac;
	}

	bool isOnEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return context.edgeLevel >= 2 && feature.chromaSq >= config_.edgeWhiteHighChromaSq && feature.brightFrac < config_.edgeWhiteMinBrightFrac;
	}

	bool isEdgeColorArtifact(const Features& feature, const SpatialContext& context) const {
		return isNearEdgeColorArtifact(feature, context) || isOnEdgeColorArtifact(feature, context);
	}

	bool isNearEdgeUnstableWhite(const Features& feature, const SpatialContext& context, float confidence) const {
		return context.edgeLevel == 1 && feature.chromaSq >= config_.edgeWhiteNearWeakChromaSq && feature.brightFrac < config_.edgeWhiteNearWeakBrightFrac &&
		       confidence < config_.edgeWhiteNearWeakMinConf;
	}

	bool failsWhiteEdgeSanity(const Features& feature, const SpatialContext& context, float confidence) const {
		return isEdgeColorArtifact(feature, context) || isNearEdgeUnstableWhite(feature, context, confidence);
	}

	bool hasEmptyRescueHint(const Features& feature, const Model& model, const Eval& evaluated) const {
		if (evaluated.state != StoneState::Empty) {
			return false;
		}
		const float z               = normalizedDelta(feature, model);
		const float brightAdvantage = feature.brightFrac - feature.darkFrac;
		return z >= config_.emptyRescueMinZ && feature.brightFrac >= config_.emptyRescueMinBright && brightAdvantage >= config_.emptyRescueMinBrightAdv;
	}

	bool hasStandardRefineHint(const Features& feature, const Model& model, const Eval& evaluated) const {
		if (evaluated.state == StoneState::Empty) {
			return false;
		}
		const float baseSupportAdv = (evaluated.state == StoneState::Black) ? (feature.darkFrac - feature.brightFrac) : (feature.brightFrac - feature.darkFrac);
		const float minAbsZ        = (evaluated.state == StoneState::White) ? config_.refineMinAbsZWhite : config_.refineMinAbsZBlack;
		const float minSupportAdv  = (evaluated.state == StoneState::White) ? config_.refineMinSupportAdvWhite : config_.refineMinSupportAdvBlack;
		const bool allowed         = std::abs(normalizedDelta(feature, model)) >= minAbsZ && baseSupportAdv >= minSupportAdv;
		return allowed && evaluated.margin < config_.refineTriggerMult * evaluated.required;
	}
};

class RefinementEngine {
public:
	bool searchBest(const cv::Point2f& intersection, const SampleContext& sampleContext, const Offsets& offsets, const Radii& radii,
	                const StoneDetectionConfig& config, const Model& model, const SpatialContext& context, double spacing, const DecisionPolicy& policy,
	                const Features& baseFeature, const Eval& baseEval, Features& outFeature, Eval& outEval) const {
		const int extent  = computeRefinementExtent(spacing, config);
		const int centerX = static_cast<int>(std::lround(intersection.x));
		const int centerY = static_cast<int>(std::lround(intersection.y));

		bool sampledAny      = false;
		Eval bestEval        = policy.decide(baseFeature, model, context, baseEval);
		Features bestFeature = baseFeature;
		for (int offsetY = -extent; offsetY <= extent; offsetY += config.refineStepPx) {
			for (int offsetX = -extent; offsetX <= extent; offsetX += config.refineStepPx) {
				if (offsetX == 0 && offsetY == 0) {
					continue;
				}
				Features candidateFeature{};
				if (!FeatureExtraction::computeFeaturesAt(sampleContext, offsets, radii, config, centerX + offsetX, centerY + offsetY, candidateFeature)) {
					continue;
				}
				sampledAny               = true;
				const Eval candidateRaw  = Scoring::evaluate(candidateFeature, model, context.edgeLevel, config);
				const Eval candidateEval = policy.decide(candidateFeature, model, context, candidateRaw);
				if (isBetterCandidate(bestEval, candidateEval, config)) {
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
	static bool isBetterCandidate(const Eval& currentBest, const Eval& candidate, const StoneDetectionConfig& config) {
		const bool betterMargin      = candidate.margin > currentBest.margin;
		const bool promotesFromEmpty = (currentBest.state == StoneState::Empty) && (candidate.state != StoneState::Empty) &&
		                               (candidate.margin + config.refinePromoteFromEmptyEps >= currentBest.margin);
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
                             const std::vector<float>& confidence, const DebugStats& stats, const std::vector<RejectionReason>* rejectionReasons = nullptr) {
	const char* debugEnv = std::getenv("GO_STONE_DEBUG");
	if (!isRuntimeDebugEnabled()) {
		return;
	}

	const std::string_view debugFlag(debugEnv);
	const bool enabled = (debugFlag == "1" || debugFlag == "2");
	if (!enabled) {
		return;
	}

	const int boardSize               = static_cast<int>(geometry.boardSize);
	const auto neighborMedianForIndex = [&](std::size_t index) {
		const int gridX = static_cast<int>(index) / boardSize;
		const int gridY = static_cast<int>(index) - gridX * boardSize;
		return computeNeighborMedianDelta(features, gridX, gridY, boardSize, model.medianEmpty);
	};

	std::cerr << "[stone-debug] N=" << geometry.boardSize << " black=" << stats.blackCount << " white=" << stats.whiteCount << " empty=" << stats.emptyCount
	          << " median=" << model.medianEmpty << " sigma=" << model.sigmaEmpty << " chromaT=" << model.tChromaSq << '\n';

	for (std::size_t index = 0; index < states.size(); ++index) {
		if (states[index] == StoneState::Empty) {
			continue;
		}
		const std::size_t gridX      = index / geometry.boardSize;
		const std::size_t gridY      = index % geometry.boardSize;
		const Features& feature      = features[index];
		const float z                = (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
		const float neighborMedian   = neighborMedianForIndex(index);
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
			const float z = (features[index].deltaL - model.medianEmpty) / model.sigmaEmpty;
			emptyRows.push_back({index, z});
		}
		std::sort(emptyRows.begin(), emptyRows.end(), [](const EmptyRow& left, const EmptyRow& right) { return left.z > right.z; });
		const std::size_t limit = std::min<std::size_t>(20, emptyRows.size());
		for (std::size_t row = 0; row < limit; ++row) {
			const std::size_t index = emptyRows[row].idx;
			const std::size_t gridX = index / geometry.boardSize;
			const std::size_t gridY = index % geometry.boardSize;
			std::cerr << "  empty-cand idx=" << index << " (" << gridX << "," << gridY << ")"
			          << " z=" << emptyRows[row].z << " d=" << features[index].darkFrac << " b=" << features[index].brightFrac
			          << " c=" << features[index].chromaSq << '\n';
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
			const float z = (features[index].deltaL - model.medianEmpty) / model.sigmaEmpty;
			rows.push_back({index, std::abs(z)});
		}
		std::sort(rows.begin(), rows.end(), [](const CandidateRow& left, const CandidateRow& right) { return left.absZ > right.absZ; });
		const std::size_t limit = std::min<std::size_t>(10, rows.size());
		for (std::size_t row = 0; row < limit; ++row) {
			const std::size_t index = rows[row].idx;
			const std::size_t gridX = index / geometry.boardSize;
			const std::size_t gridY = index % geometry.boardSize;
			const Features& feature = features[index];
			const float z           = (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
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

static void classifyAll(const std::vector<cv::Point2f>& intersections, const SampleContext& sampleContext, const Offsets& offsets, const Radii& radii,
                        const std::vector<Features>& features, const Model& model, unsigned boardSize, double spacing, const StoneDetectionConfig& config,
                        std::vector<StoneState>& outStates, std::vector<float>& outConfidence, DebugStats& outStats,
                        std::vector<RejectionReason>* outRejectionReasons = nullptr) {
	outStates.assign(intersections.size(), StoneState::Empty);
	outConfidence.assign(intersections.size(), 0.0f);
	outStats = DebugStats{};
	if (outRejectionReasons != nullptr) {
		outRejectionReasons->assign(intersections.size(), RejectionReason::None);
	}

	const int boardSizeInt                     = static_cast<int>(boardSize);
	const std::vector<float> neighborMedianMap = computeNeighborMedianMap(features, boardSizeInt, model.medianEmpty);
	const DecisionPolicy policy(config);
	const RefinementEngine refinementEngine{};

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

		const Eval baseEval = Scoring::evaluate(feature, model, context.edgeLevel, config);
		RejectionReason rejectionReason{RejectionReason::None};
		Eval decision = policy.decide(feature, model, context, baseEval, outRejectionReasons != nullptr ? &rejectionReason : nullptr);

		const DecisionPolicy::RefinementPath path = policy.refinementPath(feature, model, context, baseEval);
		if (path != DecisionPolicy::RefinementPath::None) {
			++outStats.refinedTried;
			if (policy.shouldRunRefinement(path, baseEval)) {
				Features refinedFeature = feature;
				Eval refinedEval{};
				const bool refined = refinementEngine.searchBest(intersections[index], sampleContext, offsets, radii, config, model, context, spacing, policy,
				                                                 feature, baseEval, refinedFeature, refinedEval);
				if (refined && policy.acceptsRefinement(path, baseEval, refinedFeature, refinedEval)) {
					feature  = refinedFeature;
					decision = policy.decide(feature, model, context, refinedEval, outRejectionReasons != nullptr ? &rejectionReason : nullptr);
					++outStats.refinedAccepted;
				}
			}
		}

		outStates[index]     = decision.state;
		outConfidence[index] = decision.confidence;
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

StoneResult analyseBoardV2(const BoardGeometry& geometry, DebugVisualizer* debugger) {
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

	const StoneDetectionConfig config{};
	const Radii radii     = GeometrySampling::chooseRadii(geometry.spacing, config);
	const Offsets offsets = GeometrySampling::precomputeOffsets(radii);

	LabBlur blurredLab{};
	if (!FeatureExtraction::prepareLabBlur(geometry.image, radii, config, blurredLab)) {
		if (debugger) {
			debugger->endStage();
		}
		std::cerr << "Stone detection failed: unsupported channel count\n";
		return {false, {}, {}};
	}
	const SampleContext sampleContext{blurredLab.L, blurredLab.A, blurredLab.B, blurredLab.L.rows, blurredLab.L.cols};

	const std::vector<Features> features = FeatureExtraction::computeFeatures(geometry.intersections, sampleContext, offsets, radii, config);

	Model model{};
	if (!ModelCalibration::calibrateModel(features, geometry.boardSize, config, model)) {
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
	std::vector<RejectionReason> rejectionReasons;
	const bool collectRejections = Debugging::isRuntimeDebugEnabled();
	classifyAll(geometry.intersections, sampleContext, offsets, radii, features, model, geometry.boardSize, geometry.spacing, config, states, confidence, stats,
	            collectRejections ? &rejectionReasons : nullptr);

	Debugging::emitRuntimeDebug(geometry, features, model, states, confidence, stats, collectRejections ? &rejectionReasons : nullptr);

	if (debugger) {
		debugger->add("Stone Overlay", Debugging::drawOverlay(geometry.image, geometry.intersections, states, radii.innerRadius));
		debugger->add("Stone Stats", Debugging::renderStatsTile(model, stats));
		debugger->endStage();
	}

	return {true, std::move(states), std::move(confidence)};
}

StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger) {
	return analyseBoardV2(geometry, debugger);
}

} // namespace go::camera
