#pragma once

#include "camera/debugVisualizer.hpp"
#include "camera/rectifier.hpp"

#include <opencv2/core/mat.hpp>
#include <vector>

namespace go::camera {

//! Stone state at a single grid intersection.
enum class StoneState { Empty, Black, White };

//! Result of the stone detection stage.
struct StoneResult {
	bool success;                   //!< True if detection ran successfully; false on invalid input.
	std::vector<StoneState> stones; //!< Stone states aligned to BoardGeometry::intersections (size = boardSize * boardSize).
	std::vector<float> confidence;  //!< Per-intersection confidence for stones[i] (size = stones.size()). 0 -> Empty/unknown.
};

//! Geometry sampling parameters for feature extraction.
struct GeometryConfig {
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
};

//! Robust model fitting parameters.
struct CalibrationConfig {
	float madToSigma{1.4826f};
	float sigmaMin{5.0f};
	float emptyBandSigma{1.80f};
	float likelyEmptySupportSumMax{0.35f};
	int calibMinEmptySamples{8};
	float calibMinEmptyFraction{0.10f};
	float chromaTFallback{400.0f};
	float chromaTMin{100.0f};
};

//! Score model parameters.
struct ScoringConfig {
	float scoreWDelta{1.0f};
	float scoreWSupport{0.2f};
	float scoreWChroma{0.9f};
	float margin0{1.5f};
	float edgePenalty{0.20f};
	float confChromaDownweight{0.25f};
	float emptyScoreBias{0.30f};
	float emptyScoreZPenalty{0.75f};
	float emptyScoreSupportPenalty{0.15f};
};

//! Domain gating and acceptance thresholds.
struct DecisionConfig {
	float minZBlack{3.8f};
	float minZBlackNearEdgeAdd{0.4f};
	float minZBlackOnEdgeAdd{1.2f};
	float minZWhite{0.55f};
	float minSupportBlack{0.50f};
	float minSupportWhite{0.08f};
	float minSupportAdvantageBlack{0.08f};
	float minSupportAdvantageWhite{0.03f};
	float minNeighborContrastBlack{14.0f};
	float minNeighborContrastWhite{0.0f};
	float whiteStrongAdvMin{0.06f};
	float whiteStrongNeighborMin{14.0f};
	float whiteLowChromaMax{420.0f};
	float whiteLowChromaMaxNearEdge{420.0f};
	float whiteLowChromaMinZ{0.57f};
	float whiteLowChromaMinBright{0.10f};
	float whiteLowChromaMinBrightNearEdge{0.11f};
	float edgeWhiteNearChromaSq{70.0f};
	float edgeWhiteNearMinBrightFrac{0.11f};
	float edgeWhiteNearWeakChromaSq{45.0f};
	float edgeWhiteNearWeakBrightFrac{0.11f};
	float edgeWhiteNearWeakMinConf{0.965f};
	float edgeWhiteHighChromaSq{120.0f};
	float edgeWhiteMinBrightFrac{0.35f};
	float minConfidenceBlack{0.90f};
	unsigned minConfidenceBlackBoardSize{13u};
	float minBlackMarginMult{1.0f};
	float minWhiteMarginMult{0.30f};
	float emptyRescueMinZ{0.35f};
	float emptyRescueMinBright{0.08f};
	float emptyRescueMinBrightAdv{0.02f};
	float emptyRescueMinMarginMult{0.35f};

	float refineTriggerMult{1.25f};
	float refineSkipStableEmptyMarginMult{0.80f};
	float refineAcceptGainMult{0.20f};
	float refineAcceptFromEmptyGainMult{0.10f};

	float refineMinAbsZWhite{1.2f};
	float refineMinAbsZBlack{2.0f};
	float refineMinSupportAdvWhite{0.20f};
	float refineMinSupportAdvBlack{0.35f};
};

//! Refinement search parameters.
struct RefinementConfig {
	double refineExtentSpacingK{0.09};
	int refineExtentFallback{6};
	int refineExtentMin{4};
	int refineExtentMax{8};
	int refineStepPx{2};
	float refinePromoteFromEmptyEps{1e-4f};
};

//! Full stone detection configuration.
struct StoneDetectionConfig {
	GeometryConfig geometry{};
	CalibrationConfig calibration{};
	ScoringConfig scoring{};
	DecisionConfig decision{};
	RefinementConfig refinement{};
};

/*! Detect stones on a rectified Go board image.
 * \param [in]     geometry Rectified board geometry.
 * \param [in,out] debugger Optional debug visualizer for overlays.
 * \param [in]     config   Stone detection configuration.
 * \return         StoneResult where `stones[i]`/`confidence[i]` map to `geometry.intersections[i]`.
 */
StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger = nullptr, const StoneDetectionConfig& config = StoneDetectionConfig{});

/*! Detect stones using score-based self-calibrating v2 classifier.
 * \param [in]     geometry Rectified board geometry.
 * \param [in,out] debugger Optional debug visualizer for overlays.
 * \param [in]     config   Stone detection configuration.
 * \return         StoneResult where `stones[i]`/`confidence[i]` map to `geometry.intersections[i]`.
 */
StoneResult analyseBoardV2(const BoardGeometry& geometry, DebugVisualizer* debugger = nullptr, const StoneDetectionConfig& config = StoneDetectionConfig{});

} // namespace go::camera
