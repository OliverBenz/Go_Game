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

struct GeometryParams {
	static constexpr int INNER_RADIUS_FALLBACK     = 6;
	static constexpr double INNER_RADIUS_SPACING_K = 0.24;
	static constexpr int INNER_RADIUS_MIN          = 2;
	static constexpr int INNER_RADIUS_MAX          = 30;

	static constexpr int BG_RADIUS_MIN = 2;
	static constexpr int BG_RADIUS_MAX = 12;

	static constexpr double BG_OFFSET_SPACING_K = 0.48;
	static constexpr int BG_OFFSET_MIN_EXTRA    = 2;
	static constexpr int BG_OFFSET_FALLBACK_ADD = 6;
	static constexpr int MIN_BG_SAMPLES         = 5;

	static constexpr float SUPPORT_DELTA = 18.0f;

	static constexpr float LAB_NEUTRAL          = 128.0f;
	static constexpr double BLUR_SIGMA_RADIUS_K = 0.15;
	static constexpr double BLUR_SIGMA_MIN      = 1.0;
	static constexpr double BLUR_SIGMA_MAX      = 4.0;
};

struct CalibrationParams {
	static constexpr float MAD_TO_SIGMA                 = 1.4826f;
	static constexpr float SIGMA_MIN                    = 5.0f;
	static constexpr float EMPTY_BAND_SIGMA             = 1.80f;
	static constexpr float LIKELY_EMPTY_SUPPORT_SUM_MAX = 0.35f;
	static constexpr int CALIB_MIN_EMPTY_SAMPLES        = 8;
	static constexpr float CALIB_MIN_EMPTY_FRACTION     = 0.10f;
	static constexpr float CHROMA_T_FALLBACK            = 400.0f;
	static constexpr float CHROMA_T_MIN                 = 100.0f;
};

struct ScoreParams {
	static constexpr float W_DELTA                     = 1.0f;
	static constexpr float W_SUPPORT                   = 0.2f;
	static constexpr float W_CHROMA                    = 0.9f;
	static constexpr float MARGIN0                     = 1.5f;
	static constexpr float EDGE_PENALTY                = 0.20f;
	static constexpr float CONF_CHROMA_DOWNWEIGHT      = 0.25f;
	static constexpr float EMPTY_SCORE_BIAS            = 0.30f;
	static constexpr float EMPTY_SCORE_Z_PENALTY       = 0.75f;
	static constexpr float EMPTY_SCORE_SUPPORT_PENALTY = 0.15f;

	static constexpr float MIN_Z_BLACK = 3.8f;
	static constexpr float MIN_Z_WHITE = 0.6f;

	static constexpr float MIN_SUPPORT_BLACK           = 0.50f;
	static constexpr float MIN_SUPPORT_WHITE           = 0.08f;
	static constexpr float MIN_SUPPORT_ADVANTAGE       = 0.08f;
	static constexpr float MIN_SUPPORT_ADVANTAGE_WHITE = 0.03f;

	static constexpr float MIN_NEIGHBOR_CONTRAST_BLACK = 14.0f;
	static constexpr float MIN_NEIGHBOR_CONTRAST_WHITE = 0.0f;

	static constexpr float WHITE_STRONG_ADV_MIN           = 0.08f;
	static constexpr float WHITE_STRONG_NEIGHBOR_MIN      = 12.0f;
	static constexpr float WHITE_LOW_CHROMA_MAX           = 55.0f;
	static constexpr float WHITE_LOW_CHROMA_MAX_NEAR_EDGE = 35.0f;
	static constexpr float WHITE_LOW_CHROMA_MIN_Z         = 1.0f;
	static constexpr float WHITE_LOW_CHROMA_MIN_BRIGHT    = 0.10f;

	static constexpr float MIN_CONFIDENCE_BLACK = 0.90f;
};

struct EdgeParams {
	static constexpr float EDGE_WHITE_NEAR_CHROMA_SQ        = 70.0f;
	static constexpr float EDGE_WHITE_NEAR_MIN_BRIGHT_FRAC  = 0.22f;
	static constexpr float EDGE_WHITE_NEAR_WEAK_CHROMA_SQ   = 45.0f;
	static constexpr float EDGE_WHITE_NEAR_WEAK_BRIGHT_FRAC = 0.22f;
	static constexpr float EDGE_WHITE_NEAR_WEAK_MIN_CONF    = 0.965f;
	static constexpr float EDGE_WHITE_HIGH_CHROMA_SQ        = 120.0f;
	static constexpr float EDGE_WHITE_MIN_BRIGHT_FRAC       = 0.35f;
};

struct RefinementParams {
	static constexpr float REFINE_TRIGGER_MULT      = 1.25f;
	static constexpr double REFINE_EXTENT_SPACING_K = 0.09;
	static constexpr int REFINE_EXTENT_FALLBACK     = 6;
	static constexpr int REFINE_EXTENT_MIN          = 4;
	static constexpr int REFINE_EXTENT_MAX          = 8;
	static constexpr int REFINE_STEP_PX             = 2;
	static constexpr float REFINE_ACCEPT_GAIN_MULT  = 0.20f;

	static constexpr float EMPTY_RESCUE_MIN_Z           = 0.35f;
	static constexpr float EMPTY_RESCUE_MIN_BRIGHT      = 0.08f;
	static constexpr float EMPTY_RESCUE_MIN_BRIGHT_ADV  = 0.02f;
	static constexpr float EMPTY_RESCUE_MIN_MARGIN_MULT = 0.35f;
};

struct Radii {
	int innerRadius{GeometryParams::INNER_RADIUS_FALLBACK};
	int bgRadius{GeometryParams::BG_RADIUS_MIN};
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
	float wDelta{ScoreParams::W_DELTA};
	float wSupport{ScoreParams::W_SUPPORT};
	float wChroma{ScoreParams::W_CHROMA};
	float tChromaSq{CalibrationParams::CHROMA_T_FALLBACK};
	float margin0{ScoreParams::MARGIN0};
	float edgePenalty{ScoreParams::EDGE_PENALTY};
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

struct ClassificationContext {
	unsigned boardSize{0u};
	double spacing{0.0};
	int edgeLevel{0};
	float neighborMedian{0.0f};
};

struct DebugStats {
	int blackCount{0};
	int whiteCount{0};
	int emptyCount{0};
	int refinedTried{0};
	int refinedAccepted{0};
};

namespace GeometrySampling {

static Radii chooseRadii(double spacing) {
	Radii radii{};
	int innerRadius = GeometryParams::INNER_RADIUS_FALLBACK;
	if (std::isfinite(spacing) && spacing > 0.0) {
		innerRadius = static_cast<int>(std::lround(spacing * GeometryParams::INNER_RADIUS_SPACING_K));
	}

	radii.innerRadius = std::clamp(innerRadius, GeometryParams::INNER_RADIUS_MIN, GeometryParams::INNER_RADIUS_MAX);
	radii.bgRadius    = std::clamp(radii.innerRadius / 2, GeometryParams::BG_RADIUS_MIN, GeometryParams::BG_RADIUS_MAX);

	if (std::isfinite(spacing) && spacing > 0.0) {
		const int bgOffset = static_cast<int>(std::lround(spacing * GeometryParams::BG_OFFSET_SPACING_K));
		radii.bgOffset     = std::max(bgOffset, radii.innerRadius + GeometryParams::BG_OFFSET_MIN_EXTRA);
	} else {
		radii.bgOffset = radii.innerRadius * 2 + GeometryParams::BG_OFFSET_FALLBACK_ADD;
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

static bool prepareLabBlur(const cv::Mat& image, const Radii& radii, LabBlur& outBlur) {
	cv::Mat labImage;
	if (!convertToLab(image, labImage)) {
		return false;
	}

	cv::extractChannel(labImage, outBlur.L, 0);
	cv::extractChannel(labImage, outBlur.A, 1);
	cv::extractChannel(labImage, outBlur.B, 2);

	const double sigma = std::clamp(GeometryParams::BLUR_SIGMA_RADIUS_K * static_cast<double>(radii.innerRadius), GeometryParams::BLUR_SIGMA_MIN,
	                                GeometryParams::BLUR_SIGMA_MAX);

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

static bool computeFeaturesAt(const SampleContext& context, const Offsets& offsets, const Radii& radii, int centerX, int centerY, Features& outFeatures) {
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
	if (backgroundCount < GeometryParams::MIN_BG_SAMPLES) {
		return false;
	}

	std::sort(backgroundSamples.begin(), backgroundSamples.begin() + backgroundCount);
	const float backgroundMedian = (backgroundCount % 2 == 1) ? backgroundSamples[static_cast<std::size_t>(backgroundCount / 2)]
	                                                          : 0.5f * (backgroundSamples[static_cast<std::size_t>(backgroundCount / 2 - 1)] +
	                                                                    backgroundSamples[static_cast<std::size_t>(backgroundCount / 2)]);

	outFeatures.deltaL   = innerL - backgroundMedian;
	const float deltaA   = innerA - GeometryParams::LAB_NEUTRAL;
	const float deltaB   = innerB - GeometryParams::LAB_NEUTRAL;
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
		if (diff <= -GeometryParams::SUPPORT_DELTA) {
			++dark;
		} else if (diff >= GeometryParams::SUPPORT_DELTA) {
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
                                             const Radii& radii) {
	std::vector<Features> allFeatures(intersections.size());
	for (std::size_t index = 0; index < intersections.size(); ++index) {
		const int centerX = static_cast<int>(std::lround(intersections[index].x));
		const int centerY = static_cast<int>(std::lround(intersections[index].y));
		computeFeaturesAt(context, offsets, radii, centerX, centerY, allFeatures[index]);
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

static bool robustMedianSigma(const std::vector<float>& values, float& outMedian, float& outSigma) {
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
	outSigma        = std::max(CalibrationParams::SIGMA_MIN, CalibrationParams::MAD_TO_SIGMA * mad);
	return true;
}

static bool calibrateModel(const std::vector<Features>& features, unsigned boardSize, Model& outModel) {
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
	float sigmaInitial  = CalibrationParams::SIGMA_MIN;
	if (!robustMedianSigma(allDelta, medianInitial, sigmaInitial)) {
		return false;
	}

	const float emptyBand = CalibrationParams::EMPTY_BAND_SIGMA * sigmaInitial;
	std::vector<float> likelyEmptyDelta;
	std::vector<float> likelyEmptyChroma;
	likelyEmptyDelta.reserve(allDelta.size());
	likelyEmptyChroma.reserve(allChroma.size());
	for (const Features& feature: features) {
		if (!feature.valid) {
			continue;
		}
		const bool inBand     = std::abs(feature.deltaL - medianInitial) <= emptyBand;
		const bool lowSupport = (feature.darkFrac + feature.brightFrac) <= CalibrationParams::LIKELY_EMPTY_SUPPORT_SUM_MAX;
		if (inBand && lowSupport) {
			likelyEmptyDelta.push_back(feature.deltaL);
			likelyEmptyChroma.push_back(feature.chromaSq);
		}
	}

	const std::size_t minEmptyCount = std::max<std::size_t>(
	        static_cast<std::size_t>(CalibrationParams::CALIB_MIN_EMPTY_SAMPLES),
	        static_cast<std::size_t>(std::lround(CalibrationParams::CALIB_MIN_EMPTY_FRACTION * static_cast<float>(boardSize) * static_cast<float>(boardSize))));

	float medianFinal = medianInitial;
	float sigmaFinal  = sigmaInitial;
	if (likelyEmptyDelta.size() >= minEmptyCount) {
		float medianLikely = 0.0f;
		float sigmaLikely  = 0.0f;
		if (robustMedianSigma(likelyEmptyDelta, medianLikely, sigmaLikely)) {
			medianFinal = medianLikely;
			sigmaFinal  = sigmaLikely;
		}
	}

	const std::vector<float>& chromaSource = likelyEmptyChroma.empty() ? allChroma : likelyEmptyChroma;
	float chromaMedian                     = CalibrationParams::CHROMA_T_FALLBACK;
	if (!chromaSource.empty()) {
		std::vector<float> sortedChroma = chromaSource;
		std::sort(sortedChroma.begin(), sortedChroma.end());
		chromaMedian = medianSorted(sortedChroma);
	}

	outModel.medianEmpty = medianFinal;
	outModel.sigmaEmpty  = std::max(CalibrationParams::SIGMA_MIN, sigmaFinal);
	outModel.tChromaSq   = std::max(CalibrationParams::CHROMA_T_MIN, chromaMedian);
	outModel.wDelta      = ScoreParams::W_DELTA;
	outModel.wSupport    = ScoreParams::W_SUPPORT;
	outModel.wChroma     = ScoreParams::W_CHROMA;
	outModel.margin0     = ScoreParams::MARGIN0;
	outModel.edgePenalty = ScoreParams::EDGE_PENALTY;
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

static Scores computeScores(const Features& feature, const Model& model) {
	Scores scores{};
	scores.z                 = (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	const float supportBlack = feature.darkFrac - feature.brightFrac;
	const float supportWhite = feature.brightFrac - feature.darkFrac;
	scores.chromaPenalty     = feature.chromaSq / (model.tChromaSq + feature.chromaSq);

	scores.black = model.wDelta * (-scores.z) + model.wSupport * supportBlack - model.wChroma * scores.chromaPenalty;
	scores.white = model.wDelta * (scores.z) + model.wSupport * supportWhite - model.wChroma * scores.chromaPenalty;
	scores.empty = ScoreParams::EMPTY_SCORE_BIAS - ScoreParams::EMPTY_SCORE_Z_PENALTY * std::abs(scores.z) -
	               ScoreParams::EMPTY_SCORE_SUPPORT_PENALTY * (feature.darkFrac + feature.brightFrac);
	return scores;
}

static Eval evaluate(const Features& feature, const Model& model, int edgeLevelValue) {
	const Scores scores = computeScores(feature, model);
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
	result.confidence *= std::clamp(1.0f - ScoreParams::CONF_CHROMA_DOWNWEIGHT * scores.chromaPenalty, 0.0f, 1.0f);
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

class DecisionPolicy {
public:
	enum class RefinementPath { None, EmptyRescue, Standard };

	Eval decide(const Features& feature, const Model& model, const ClassificationContext& context, const Eval& evaluated) const {
		Eval decision = evaluated;
		const float z = normalizedDelta(feature, model);

		if (isWeakByZ(decision.state, z, context.edgeLevel)) {
			clearDecision(decision);
		}
		if (decision.state == StoneState::Black && failsBlackConfidence(decision, context.boardSize)) {
			clearDecision(decision);
		}
		if (decision.state == StoneState::Black && failsBlackSanity(feature, context)) {
			clearDecision(decision);
		}
		if (decision.state == StoneState::White && failsWhiteSanity(feature, context, z, decision.confidence)) {
			clearDecision(decision);
		}
		if (decision.state == StoneState::Black && decision.margin < decision.required) {
			clearDecision(decision);
		}
		if (decision.state == StoneState::White && decision.margin < 0.30f * decision.required) {
			clearDecision(decision);
		}
		return decision;
	}

	Eval decide(const Features& feature, const Model& model, const ClassificationContext& context) const {
		return decide(feature, model, context, Scoring::evaluate(feature, model, context.edgeLevel));
	}

	RefinementPath refinementPath(const Features& feature, const Model& model, const ClassificationContext&, const Eval& evaluated) const {
		if (hasEmptyRescueHint(feature, model, evaluated)) {
			return RefinementPath::EmptyRescue;
		}
		if (hasStandardRefineHint(feature, model, evaluated)) {
			return RefinementPath::Standard;
		}
		return RefinementPath::None;
	}

	bool acceptRefinement(RefinementPath path, const Features& refinedFeature, const Eval& refinedEval) const {
		if (path == RefinementPath::Standard) {
			return true;
		}
		if (path == RefinementPath::None) {
			return false;
		}
		return refinedEval.state == StoneState::White && refinedEval.margin >= RefinementParams::EMPTY_RESCUE_MIN_MARGIN_MULT * refinedEval.required &&
		       (refinedFeature.brightFrac - refinedFeature.darkFrac) >= ScoreParams::MIN_SUPPORT_ADVANTAGE_WHITE;
	}

private:
	static float normalizedDelta(const Features& feature, const Model& model) {
		return (feature.deltaL - model.medianEmpty) / model.sigmaEmpty;
	}

	static void clearDecision(Eval& decision) {
		decision.state      = StoneState::Empty;
		decision.confidence = 0.0f;
	}

	static bool isWeakByZ(StoneState state, float z, int edgeLevelValue) {
		const float minBlackZ = ScoreParams::MIN_Z_BLACK + (edgeLevelValue == 1 ? 0.4f : (edgeLevelValue == 2 ? 1.2f : 0.0f));
		return (state == StoneState::Black && (-z) < minBlackZ) || (state == StoneState::White && z < ScoreParams::MIN_Z_WHITE);
	}

	static bool failsBlackConfidence(const Eval& decision, unsigned boardSize) {
		const float threshold = (boardSize >= 13u) ? ScoreParams::MIN_CONFIDENCE_BLACK : 0.0f;
		return decision.confidence < threshold;
	}

	static bool failsBlackSanity(const Features& feature, const ClassificationContext& context) {
		const bool weakSupport  = feature.darkFrac < ScoreParams::MIN_SUPPORT_BLACK;
		const bool weakContrast = (feature.darkFrac - feature.brightFrac) < ScoreParams::MIN_SUPPORT_ADVANTAGE;
		const bool weakNeighbor = (context.neighborMedian - feature.deltaL) < ScoreParams::MIN_NEIGHBOR_CONTRAST_BLACK;
		return weakSupport || weakContrast || weakNeighbor;
	}

	static bool hasStrongWhiteSupport(const Features& feature, const ClassificationContext& context) {
		const float brightAdvantage  = feature.brightFrac - feature.darkFrac;
		const float neighborContrast = feature.deltaL - context.neighborMedian;
		return brightAdvantage >= ScoreParams::WHITE_STRONG_ADV_MIN && neighborContrast >= ScoreParams::WHITE_STRONG_NEIGHBOR_MIN;
	}

	static bool qualifiesLowChromaRescue(const Features& feature, const ClassificationContext& context, float z) {
		const float chromaCap = (context.edgeLevel == 1) ? ScoreParams::WHITE_LOW_CHROMA_MAX_NEAR_EDGE : ScoreParams::WHITE_LOW_CHROMA_MAX;
		const float minBright = (context.edgeLevel == 1) ? 0.16f : ScoreParams::WHITE_LOW_CHROMA_MIN_BRIGHT;
		return feature.chromaSq <= chromaCap && z >= ScoreParams::WHITE_LOW_CHROMA_MIN_Z && feature.brightFrac >= minBright;
	}

	static bool isNearEdgeColorArtifact(const Features& feature, const ClassificationContext& context) {
		return context.edgeLevel == 1 && feature.chromaSq >= EdgeParams::EDGE_WHITE_NEAR_CHROMA_SQ &&
		       feature.brightFrac < EdgeParams::EDGE_WHITE_NEAR_MIN_BRIGHT_FRAC;
	}

	static bool isOnEdgeColorArtifact(const Features& feature, const ClassificationContext& context) {
		return context.edgeLevel >= 2 && feature.chromaSq >= EdgeParams::EDGE_WHITE_HIGH_CHROMA_SQ &&
		       feature.brightFrac < EdgeParams::EDGE_WHITE_MIN_BRIGHT_FRAC;
	}

	static bool isEdgeColorArtifact(const Features& feature, const ClassificationContext& context) {
		return isNearEdgeColorArtifact(feature, context) || isOnEdgeColorArtifact(feature, context);
	}

	static bool isNearEdgeUnstableWhite(const Features& feature, const ClassificationContext& context, float confidence) {
		return context.edgeLevel == 1 && feature.chromaSq >= EdgeParams::EDGE_WHITE_NEAR_WEAK_CHROMA_SQ &&
		       feature.brightFrac < EdgeParams::EDGE_WHITE_NEAR_WEAK_BRIGHT_FRAC && confidence < EdgeParams::EDGE_WHITE_NEAR_WEAK_MIN_CONF;
	}

	static bool failsWhiteSanity(const Features& feature, const ClassificationContext& context, float z, float confidence) {
		if (feature.brightFrac < ScoreParams::MIN_SUPPORT_WHITE) {
			return true;
		}
		if (!hasStrongWhiteSupport(feature, context) && !qualifiesLowChromaRescue(feature, context, z)) {
			return true;
		}
		return isEdgeColorArtifact(feature, context) || isNearEdgeUnstableWhite(feature, context, confidence);
	}

	static bool hasEmptyRescueHint(const Features& feature, const Model& model, const Eval& evaluated) {
		if (evaluated.state != StoneState::Empty) {
			return false;
		}
		const float z               = normalizedDelta(feature, model);
		const float brightAdvantage = feature.brightFrac - feature.darkFrac;
		return z >= RefinementParams::EMPTY_RESCUE_MIN_Z && feature.brightFrac >= RefinementParams::EMPTY_RESCUE_MIN_BRIGHT &&
		       brightAdvantage >= RefinementParams::EMPTY_RESCUE_MIN_BRIGHT_ADV;
	}

	static bool hasStandardRefineHint(const Features& feature, const Model& model, const Eval& evaluated) {
		if (evaluated.state == StoneState::Empty) {
			return false;
		}
		const float baseSupportAdv = (evaluated.state == StoneState::Black) ? (feature.darkFrac - feature.brightFrac) : (feature.brightFrac - feature.darkFrac);
		const float minAbsZ        = (evaluated.state == StoneState::White) ? 1.2f : 2.0f;
		const float minSupportAdv  = (evaluated.state == StoneState::White) ? 0.20f : 0.35f;
		const bool allowed         = std::abs(normalizedDelta(feature, model)) >= minAbsZ && baseSupportAdv >= minSupportAdv;
		return allowed && evaluated.margin < RefinementParams::REFINE_TRIGGER_MULT * evaluated.required;
	}
};

class RefinementEngine {
public:
	bool tryRefine(const cv::Point2f& intersection, const SampleContext& sampleContext, const Offsets& offsets, const Radii& radii, const Model& model,
	               const ClassificationContext& context, const Eval& baseEval, Features& inOutFeature, Eval& outEval) const {
		if (skipStableStone(baseEval) || skipStableEmpty(baseEval)) {
			outEval = baseEval;
			return false;
		}

		const int extent  = refinementExtent(context.spacing);
		const int centerX = static_cast<int>(std::lround(intersection.x));
		const int centerY = static_cast<int>(std::lround(intersection.y));

		Eval bestEval        = baseEval;
		Features bestFeature = inOutFeature;
		for (int offsetY = -extent; offsetY <= extent; offsetY += RefinementParams::REFINE_STEP_PX) {
			for (int offsetX = -extent; offsetX <= extent; offsetX += RefinementParams::REFINE_STEP_PX) {
				if (offsetX == 0 && offsetY == 0) {
					continue;
				}
				Features candidateFeature{};
				if (!FeatureExtraction::computeFeaturesAt(sampleContext, offsets, radii, centerX + offsetX, centerY + offsetY, candidateFeature)) {
					continue;
				}
				const Eval candidateEval = Scoring::evaluate(candidateFeature, model, context.edgeLevel);
				if (isBetterCandidate(bestEval, candidateEval)) {
					bestEval    = candidateEval;
					bestFeature = candidateFeature;
				}
			}
		}

		if (baseEval.state == StoneState::Empty) {
			return acceptFromEmpty(baseEval, bestEval, bestFeature, inOutFeature, outEval);
		}
		return acceptFromStone(baseEval, bestEval, bestFeature, inOutFeature, outEval);
	}

private:
	static bool skipStableStone(const Eval& baseEval) {
		return baseEval.state != StoneState::Empty && baseEval.margin >= RefinementParams::REFINE_TRIGGER_MULT * baseEval.required;
	}

	static bool skipStableEmpty(const Eval& baseEval) {
		return baseEval.state == StoneState::Empty && baseEval.margin >= 0.80f * baseEval.required;
	}

	static int refinementExtent(double spacing) {
		int extent = RefinementParams::REFINE_EXTENT_FALLBACK;
		if (std::isfinite(spacing) && spacing > 0.0) {
			extent = static_cast<int>(std::lround(spacing * RefinementParams::REFINE_EXTENT_SPACING_K));
		}
		return std::clamp(extent, RefinementParams::REFINE_EXTENT_MIN, RefinementParams::REFINE_EXTENT_MAX);
	}

	static bool isBetterCandidate(const Eval& currentBest, const Eval& candidate) {
		const bool betterMargin = candidate.margin > currentBest.margin;
		const bool promotesFromEmpty =
		        (currentBest.state == StoneState::Empty) && (candidate.state != StoneState::Empty) && (candidate.margin + 1e-4f >= currentBest.margin);
		return betterMargin || promotesFromEmpty;
	}

	static bool acceptFromEmpty(const Eval& baseEval, const Eval& bestEval, const Features& bestFeature, Features& inOutFeature, Eval& outEval) {
		const float minGain = 0.10f * baseEval.required;
		if (bestEval.state != StoneState::Empty && bestEval.margin > baseEval.margin + minGain) {
			inOutFeature = bestFeature;
			outEval      = bestEval;
			return true;
		}
		outEval = baseEval;
		return false;
	}

	static bool acceptFromStone(const Eval& baseEval, const Eval& bestEval, const Features& bestFeature, Features& inOutFeature, Eval& outEval) {
		const float requiredGain = RefinementParams::REFINE_ACCEPT_GAIN_MULT * baseEval.required;
		if (bestEval.margin > baseEval.margin + requiredGain) {
			inOutFeature = bestFeature;
			outEval      = bestEval;
			return true;
		}
		outEval = baseEval;
		return false;
	}
};

namespace Debugging {

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
                             const std::vector<float>& confidence, const DebugStats& stats) {
	const char* debugEnv = std::getenv("GO_STONE_DEBUG");
	if (debugEnv == nullptr) {
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
}

} // namespace Debugging

static void classifyAll(const std::vector<cv::Point2f>& intersections, const SampleContext& sampleContext, const Offsets& offsets, const Radii& radii,
                        const std::vector<Features>& features, const Model& model, unsigned boardSize, double spacing, std::vector<StoneState>& outStates,
                        std::vector<float>& outConfidence, DebugStats& outStats) {
	outStates.assign(intersections.size(), StoneState::Empty);
	outConfidence.assign(intersections.size(), 0.0f);
	outStats = DebugStats{};

	const int boardSizeInt = static_cast<int>(boardSize);
	const DecisionPolicy policy{};
	const RefinementEngine refinementEngine{};

	for (std::size_t index = 0; index < intersections.size(); ++index) {
		if (!features[index].valid) {
			++outStats.emptyCount;
			continue;
		}

		Features feature = features[index];
		const int gridX  = static_cast<int>(index) / boardSizeInt;
		const int gridY  = static_cast<int>(index) - gridX * boardSizeInt;
		const ClassificationContext context{boardSize, spacing, Scoring::edgeLevel(index, boardSizeInt),
		                                    computeNeighborMedianDelta(features, gridX, gridY, boardSizeInt, model.medianEmpty)};

		const Eval baseEval = Scoring::evaluate(feature, model, context.edgeLevel);
		Eval decision       = policy.decide(feature, model, context, baseEval);

		const DecisionPolicy::RefinementPath path = policy.refinementPath(feature, model, context, baseEval);
		if (path != DecisionPolicy::RefinementPath::None) {
			++outStats.refinedTried;
			Features refinedFeature = feature;
			Eval refinedEval{};
			const bool refined =
			        refinementEngine.tryRefine(intersections[index], sampleContext, offsets, radii, model, context, baseEval, refinedFeature, refinedEval);
			if (refined && policy.acceptRefinement(path, refinedFeature, refinedEval)) {
				feature  = refinedFeature;
				decision = policy.decide(feature, model, context, refinedEval);
				++outStats.refinedAccepted;
			}
		}

		outStates[index]     = decision.state;
		outConfidence[index] = decision.confidence;
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

	const Radii radii     = GeometrySampling::chooseRadii(geometry.spacing);
	const Offsets offsets = GeometrySampling::precomputeOffsets(radii);

	LabBlur blurredLab{};
	if (!FeatureExtraction::prepareLabBlur(geometry.image, radii, blurredLab)) {
		if (debugger) {
			debugger->endStage();
		}
		std::cerr << "Stone detection failed: unsupported channel count\n";
		return {false, {}, {}};
	}
	const SampleContext sampleContext{blurredLab.L, blurredLab.A, blurredLab.B, blurredLab.L.rows, blurredLab.L.cols};

	const std::vector<Features> features = FeatureExtraction::computeFeatures(geometry.intersections, sampleContext, offsets, radii);

	Model model{};
	if (!ModelCalibration::calibrateModel(features, geometry.boardSize, model)) {
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
	classifyAll(geometry.intersections, sampleContext, offsets, radii, features, model, geometry.boardSize, geometry.spacing, states, confidence, stats);

	Debugging::emitRuntimeDebug(geometry, features, model, states, confidence, stats);

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
