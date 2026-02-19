#include "camera/stoneFinder.hpp"

#include "camera/rectifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

/*! Classify Go stones (Black/White/Empty) at grid intersections on a rectified board image.
 *
 * The rectifier produces a perspective-corrected board image and the list of intersection coordinates
 * (in that rectified image). We classify stones by sampling small circular ROIs (regions of interest) around each intersection
 * in Lab color space and comparing their lightness (L) to a local background estimate.
 *
 * Design notes (why this approach):
 * - Lab L is a more perceptually linear brightness measure than BGR channels.
 * - Using deltaL = L(intersection) - median(L(background samples)) makes the detector robust to
 *   illumination gradients across the board.
 * - Robust statistics (median + MAD) avoid brittle global thresholds and reduce false positives.
 * - Chroma (a/b distance to neutral) helps reject wood grain / colored artifacts (stones are near-neutral).
 */
namespace go::camera {

namespace {

/*! Tunable parameters for stone detection.
 *
 * Goal: keep every threshold and "magic number" in one place to make tuning and review easy.
 */
struct Params {
	// Radii selection (derived from grid spacing).
	static constexpr int INNER_RADIUS_FALLBACK     = 6;    //!< ROI radius fallback when spacing is unknown (px)
	static constexpr double INNER_RADIUS_SPACING_K = 0.28; //!< Magic 28% to get all tests working...
	static constexpr int INNER_RADIUS_MIN          = 2;    //!< Minimum ROI radius (px)
	static constexpr int INNER_RADIUS_MAX          = 30;   //!< Maximum ROI radius (px)

	static constexpr int BG_RADIUS_MIN = 2;  //!< Minimum background ROI radius (px)
	static constexpr int BG_RADIUS_MAX = 12; //!< Maximum background ROI radius (px)

	static constexpr double BG_OFFSET_SPACING_K = 0.48; //!< Background sample distance as fraction of spacing
	static constexpr int BG_OFFSET_MIN_EXTRA    = 2;    //!< Ensure bgOffset >= innerRadius + this (px)
	static constexpr int BG_OFFSET_FALLBACK_ADD = 6;    //!< bgOffset fallback addend (px)
	static constexpr int MIN_BG_SAMPLES         = 2;    //!< Need at least this many background samples for a median

	// Lab constants (OpenCV Lab uses [0..255], with neutral a/b around 128).
	static constexpr float LAB_NEUTRAL = 128.0f; //!< Neutral a/b value

	// Blur (suppresses thin grid lines before sampling).
	static constexpr double BLUR_SIGMA_RADIUS_K = 0.15; //!< Sigma factor relative to innerRadius
	static constexpr double BLUR_SIGMA_MIN      = 1.0;  //!< Minimum blur sigma (px)
	static constexpr double BLUR_SIGMA_MAX      = 4.0;  //!< Maximum blur sigma (px)

	// Feature support (per-pixel delta around local background).
	static constexpr float SUPPORT_DELTA_L = 10.0f; //!< L threshold (Lab units) relative to local bg median

	// Robust thresholding (median + MAD).
	static constexpr float MAD_TO_STD                        = 1.4826f; //!< MAD -> sigma for Gaussian
	static constexpr float ROBUST_STD_FLOOR                  = 1.0f;    //!< Lower floor for robustStd
	static constexpr int MIN_NOISE_SAMPLES                   = 8;       //!< Minimum samples for noise-only (stone-trimmed) MAD estimate
	static constexpr float NOISE_REESTIMATE_REMOVED_FRAC_MIN = 0.30f;   //!< Only re-estimate from noise when many points look like stones

	static constexpr float MIN_ABS_CONTRAST_BLACK = 17.0f; //!< Minimum |deltaL| for black stones (Lab L units)
	static constexpr float MIN_ABS_CONTRAST_WHITE = 11.0f; //!< Minimum |deltaL| for white stones (weaker contrast)
	static constexpr float K_SIGMA                = 3.0f;  //!< Noise multiplier for thresholding
	static constexpr float BLACK_BIAS             = 6.0f;  //!< Extra strictness for black: shadows/wood grain are more likely to look "dark" than "white"

	static constexpr float NEIGHBOR_MARGIN_MIN      = 3.0f; //!< Minimum neighbor margin (Lab L units)
	static constexpr float NEIGHBOR_MARGIN_STD_MULT = 1.2f; //!< neighborMargin = max(min, mult * robustStd)

	static constexpr float CONFIDENCE_MAX = 10.0f; //!< Confidence clamp (sigma units), used for debug/telemetry

	// Chroma gating.
	static constexpr float MAX_CHROMA_SQ_HARD  = 30.0f * 30.0f; //!< Hard reject extreme color (strong wood/ink artifacts)
	static constexpr float MAX_CHROMA_SQ_BLACK = 200.0f;        //!< Looser than white (black stones can pick up reflections)
	static constexpr float MAX_CHROMA_SQ_WHITE = 20.0f * 20.0f; //!< Normal white chroma cap
	static constexpr float MAX_CHROMA_SQ_WHITE_STRICT =
	        10.0f * 10.0f; //!< When deltaL is only barely white, require very low chroma to avoid wood/tint false positives

	// Support fractions.
	static constexpr float SUPPORT_DOMINANCE_MARGIN = 0.10f; //!< Require winner support > loser support + margin
	static constexpr float MIN_SUPPORT_FRAC_BLACK   = 0.35f; //!< Require dark support over a decent ROI fraction
	static constexpr float MIN_SUPPORT_FRAC_WHITE   = 0.40f; //!< Require strong bright support

	// White refinement parameters.
	static constexpr float REFINE_RANGE_ON_EDGE   = 6.0f;  //!< deltaL window to trigger refinement at border
	static constexpr float REFINE_RANGE_INTERIOR  = 4.5f;  //!< deltaL window to trigger refinement in interior
	static constexpr float REFINE_ACTIVATION_BAND = 8.0f;  //!< Only run refinement when deltaL is near baseWhite (performance guard)
	static constexpr float REFINE_EXTRA_RANGE     = 0.6f;  //!< widen refinement trigger if already bright-supported
	static constexpr float REFINE_EXTRA_RANGE_BF  = 0.35f; //!< bf threshold to enable extraRange
	static constexpr float REFINE_MIN_PRE_BF_EDGE = 0.25f; //!< minimum bf to consider borderline refinement at border
	static constexpr float REFINE_MIN_PRE_BF      = 0.28f; //!< minimum bf to consider borderline refinement

	static constexpr float BORDERLINE_MAX_CHROMA_SQ = 10.0f * 10.0f; //!< chroma cap for borderline refinement trigger

	static constexpr int REFINE_STEP_PX        = 2;  //!< Pixel step (trade-off speed vs precision)
	static constexpr int REFINE_EXTENT_ON_EDGE = 12; //!< Search extent (px) at border
	static constexpr int REFINE_EXTENT_NEAR    = 8;  //!< Search extent (px) near border
	static constexpr int REFINE_EXTENT         = 6;  //!< Search extent (px) in interior

	// Edge-specific chroma handling for whites.
	//! Note: all chroma thresholds are expressed in chromaSq units: (a-128)^2 + (b-128)^2.
	static constexpr float EDGE_CHROMA_SQ_CAP_ON_EDGE   = 150.0f; //!< chromaSq cap on the outermost ring
	static constexpr float EDGE_CHROMA_SQ_CAP_NEAR_EDGE = 200.0f; //!< chromaSq cap in the 2-wide border
	static constexpr float WEAK_WHITE_MARGIN            = 3.0f;   //!< "weak" = near baseWhite

	static constexpr float EDGE_WEAK_MARGIN        = 2.0f;  //!< Near edges, background estimates are less reliable; require extra contrast for weak whites
	static constexpr float EDGE_MAX_CHROMA_SQ_WEAK = 70.0f; //!< Near edges, weak whites must be near-neutral; colored border/padding should be rejected
};

//! Radii and sampling distances (in pixels) derived from grid spacing.
struct Radii {
	int innerRadius{Params::INNER_RADIUS_FALLBACK}; //!< filled ROI radius at intersection (px)
	int bgRadius{Params::BG_RADIUS_MIN};            //!< filled ROI radius for background samples (px)
	int bgOffset{0};                                //!< distance of background samples from intersection (px)
};

//! Precomputed filled-circle offsets to avoid per-sample mask allocations.
struct Offsets {
	std::vector<cv::Point> inner; //!< integer (dx,dy) offsets for inner ROI
	std::vector<cv::Point> bg;    //!< integer (dx,dy) offsets for background ROIs
};

//! Blurred Lab channels used for sampling.
struct LabBlur {
	cv::Mat L; //!< blurred lightness (CV_8U)
	cv::Mat A; //!< blurred a (CV_8U)
	cv::Mat B; //!< blurred b (CV_8U)
};

//! Per-intersection features used for classification.
struct Features {
	float deltaL{0.0f};     //!< local-normalized lightness (innerL - bgMedianL)
	float chromaSq{0.0f};   //!< (a-128)^2 + (b-128)^2 at the intersection ROI
	float darkFrac{0.0f};   //!< fraction of ROI pixels with L <= bgMedian - SUPPORT_DELTA_L
	float brightFrac{0.0f}; //!< fraction of ROI pixels with L >= bgMedian + SUPPORT_DELTA_L
	bool valid{false};      //!< true if sampling succeeded (enough in-bounds pixels + bg samples)
};

//! Robust thresholds derived from the deltaL distribution.
struct Thresholds {
	float medianDeltaL{0.0f};   //!< median deltaL of valid intersections
	float robustStd{0.0f};      //!< robust noise estimate (MAD * MAD_TO_STD)
	float baseBlack{0.0f};      //!< coarse black candidate cutoff (deltaL <= baseBlack)
	float baseWhite{0.0f};      //!< coarse white candidate cutoff (deltaL >= baseWhite)
	float neighborMargin{0.0f}; //!< local contrast margin vs neighbor median
};

//! Coarse candidate classification result (type + optional strength for future scoring).
struct Candidate {
	StoneState type{StoneState::Empty}; //!< candidate type (Empty/Black/White)
	float strength{0.0f};               //!< For Black: (baseBlack - deltaL). For White: (deltaL - baseWhite). (>= 0 for non-empty)
};

//! Edge-specific parameters grouped into a single profile (interior vs near-edge vs on-edge).
struct EdgeProfile {
	float chromaCap{Params::MAX_CHROMA_SQ_WHITE};     //!< max allowed chromaSq for whites in this region
	float weakMargin{0.0f};                           //!< extra deltaL margin for weak whites (0 disables)
	float refineRange{Params::REFINE_RANGE_INTERIOR}; //!< deltaL window to trigger refinement
	int refineExtent{Params::REFINE_EXTENT};          //!< refinement search extent (px)
	float minPreBright{Params::REFINE_MIN_PRE_BF};    //!< minimum brightFrac to attempt borderline refinement
};

//! Centralized chroma thresholds (kept separate from edge-specific caps).
struct ChromaProfile {
	float hardCap{Params::MAX_CHROMA_SQ_HARD};                //!< absolute reject cap
	float blackCap{Params::MAX_CHROMA_SQ_BLACK};              //!< black-stone chroma cap
	float whiteCap{Params::MAX_CHROMA_SQ_WHITE};              //!< white-stone chroma cap
	float strictWhiteCap{Params::MAX_CHROMA_SQ_WHITE_STRICT}; //!< strict cap for weak-contrast whites
};

//! Optional instrumentation: counters for understanding classifier behavior on real data.
struct DetectionStats {
	int total{0};               //!< #valid intersections processed
	int classifiedBlack{0};     //!< #final black stones
	int classifiedWhite{0};     //!< #final white stones
	int rejectedChroma{0};      //!< #rejections due to chroma gating (incl. hard cap)
	int rejectedSupport{0};     //!< #rejections due to support gating
	int rejectedNeighbor{0};    //!< #rejections due to neighbor gating
	int refinementTriggered{0}; //!< #times refinement search was attempted
	int refinementAccepted{0};  //!< #times refinement found a better white candidate
};

//! References to the blurred Lab channels plus image bounds for sampling.
struct SampleContext {
	const cv::Mat& L; //!< blurred L channel (CV_8U)
	const cv::Mat& A; //!< blurred A channel (CV_8U)
	const cv::Mat& B; //!< blurred B channel (CV_8U)
	int rows{0};      //!< image height
	int cols{0};      //!< image width
};

/*! Convert input image to Lab (CV_8UC3).
 * \return True if conversion succeeded; false for unsupported channel counts.
 */
static bool convertToLab(const cv::Mat& image, cv::Mat& outLab) {
	if (image.channels() == 3) {
		cv::cvtColor(image, outLab, cv::COLOR_BGR2Lab);
		return true;
	}
	if (image.channels() == 4) {
		cv::Mat bgr; //!< Temporary 3-channel conversion (BGRA -> BGR)
		cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
		cv::cvtColor(bgr, outLab, cv::COLOR_BGR2Lab);
		return true;
	}
	if (image.channels() == 1) {
		cv::Mat bgr; //!< Temporary 3-channel conversion (gray -> BGR)
		cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
		cv::cvtColor(bgr, outLab, cv::COLOR_BGR2Lab);
		return true;
	}
	return false;
}

/*! Choose radii and sampling distances from the estimated grid spacing.
 * \param [in] spacing Pixels between adjacent grid intersections (refined coordinates).
 */
static Radii chooseRadii(double spacing) {
	Radii r{};

	// 1) Inner ROI radius (tracks stone size).
	int inner = Params::INNER_RADIUS_FALLBACK; //!< radius fallback (px)
	if (std::isfinite(spacing) && spacing > 0.0) {
		inner = static_cast<int>(std::lround(spacing * Params::INNER_RADIUS_SPACING_K));
	}
	inner         = std::clamp(inner, Params::INNER_RADIUS_MIN, Params::INNER_RADIUS_MAX);
	r.innerRadius = inner;

	// 2) Background ROI radius (smaller circle, sampling wood around the stone).
	r.bgRadius = std::clamp(inner / 2, Params::BG_RADIUS_MIN, Params::BG_RADIUS_MAX);

	// 3) Background sample distance (keep samples outside the stone ROI).
	if (std::isfinite(spacing) && spacing > 0.0) {
		const int d = static_cast<int>(std::lround(spacing * Params::BG_OFFSET_SPACING_K));
		r.bgOffset  = std::max(d, inner + Params::BG_OFFSET_MIN_EXTRA);
	} else {
		r.bgOffset = inner * 2 + Params::BG_OFFSET_FALLBACK_ADD;
	}
	return r;
}

//! Precompute (dx,dy) offsets for a filled circle ROI.
static std::vector<cv::Point> makeCircleOffsets(int radius) {
	std::vector<cv::Point> offsets; //!< integer (dx,dy) offsets inside a filled circle
	offsets.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
	const int r2 = radius * radius; //!< radius squared

	for (int dy = -radius; dy <= radius; ++dy) {
		for (int dx = -radius; dx <= radius; ++dx) {
			if ((dx * dx + dy * dy) <= r2) {
				offsets.emplace_back(dx, dy);
			}
		}
	}
	return offsets;
}

//! Precompute offsets for inner and background ROIs.
static Offsets precomputeOffsets(const Radii& radii) {
	Offsets out{};
	out.inner = makeCircleOffsets(radii.innerRadius);
	out.bg    = (radii.bgRadius == radii.innerRadius) ? out.inner : makeCircleOffsets(radii.bgRadius);
	return out;
}

/*! Convert image to Lab, extract channels, and blur them to suppress thin grid lines.
 * \param [in]  image Rectified board image.
 * \param [in]  radii Sampling radii (used to select blur sigma).
 * \param [out] outBlur Blurred Lab channels.
 */
static bool prepareLabBlur(const cv::Mat& image, const Radii& radii, LabBlur& outBlur) {
	cv::Mat lab; //!< Lab image (CV_8UC3)
	if (!convertToLab(image, lab)) {
		return false;
	}

	cv::Mat L; //!< lightness channel (CV_8U)
	cv::Mat A; //!< a channel (CV_8U)
	cv::Mat B; //!< b channel (CV_8U)
	cv::extractChannel(lab, L, 0);
	cv::extractChannel(lab, A, 1);
	cv::extractChannel(lab, B, 2);

	const double sigma = std::clamp(Params::BLUR_SIGMA_RADIUS_K * static_cast<double>(radii.innerRadius), Params::BLUR_SIGMA_MIN, Params::BLUR_SIGMA_MAX);
	cv::GaussianBlur(L, outBlur.L, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	cv::GaussianBlur(A, outBlur.A, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	cv::GaussianBlur(B, outBlur.B, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	return true;
}

/*! Sample mean L inside a filled circle.
 * Bounds-safe: out-of-image pixels are skipped.
 */
static bool sampleMeanL(const SampleContext& ctx, int cx, int cy, const std::vector<cv::Point>& offsets, float& outMean) {
	std::uint32_t sum   = 0; //!< sum of L values
	std::uint32_t count = 0; //!< number of in-bounds pixels
	for (const auto& off: offsets) {
		const int x = cx + off.x; //!< pixel x
		const int y = cy + off.y; //!< pixel y
		if (x < 0 || x >= ctx.cols || y < 0 || y >= ctx.rows) {
			continue;
		}
		sum += static_cast<std::uint32_t>(ctx.L.ptr<std::uint8_t>(y)[x]);
		++count;
	}
	if (count == 0) {
		return false;
	}
	outMean = static_cast<float>(sum) / static_cast<float>(count);
	return true;
}

/*! Sample mean Lab inside a filled circle.
 * Bounds-safe: out-of-image pixels are skipped.
 */
static bool sampleMeanLab(const SampleContext& ctx, int cx, int cy, const std::vector<cv::Point>& offsets, float& outL, float& outA, float& outB) {
	std::uint32_t sumL  = 0; //!< sum of L values
	std::uint32_t sumA  = 0; //!< sum of a values
	std::uint32_t sumB  = 0; //!< sum of b values
	std::uint32_t count = 0; //!< number of in-bounds pixels
	for (const auto& off: offsets) {
		const int x = cx + off.x; //!< pixel x
		const int y = cy + off.y; //!< pixel y
		if (x < 0 || x >= ctx.cols || y < 0 || y >= ctx.rows) {
			continue;
		}
		sumL += static_cast<std::uint32_t>(ctx.L.ptr<std::uint8_t>(y)[x]);
		sumA += static_cast<std::uint32_t>(ctx.A.ptr<std::uint8_t>(y)[x]);
		sumB += static_cast<std::uint32_t>(ctx.B.ptr<std::uint8_t>(y)[x]);
		++count;
	}
	if (count == 0) {
		return false;
	}
	outL = static_cast<float>(sumL) / static_cast<float>(count);
	outA = static_cast<float>(sumA) / static_cast<float>(count);
	outB = static_cast<float>(sumB) / static_cast<float>(count);
	return true;
}

/*! Compute per-intersection features at an arbitrary pixel position (cx,cy).
 *
 * This is used both for the main per-intersection feature extraction and for the white-stone refinement search,
 * avoiding duplicated feature logic.
 */
static bool computeFeaturesAt(const SampleContext& ctx, const Offsets& offsets, const Radii& radii, int cx, int cy, Features& out) {
	out = Features{};

	// 1) Mean Lab in the inner circle.
	float innerL = 0.0f; //!< mean L at (cx,cy)
	float innerA = 0.0f; //!< mean a at (cx,cy)
	float innerB = 0.0f; //!< mean b at (cx,cy)
	if (!sampleMeanLab(ctx, cx, cy, offsets.inner, innerL, innerA, innerB)) {
		return false;
	}

	// 2) Local background median from 8 surrounding circles.
	std::array<float, 8> bgVals{}; //!< background L means from 8 surrounding samples
	int bgCount = 0;               //!< number of successful background samples

	const int dx = radii.bgOffset; //!< background offset in x (px)
	const int dy = radii.bgOffset; //!< background offset in y (px)
	float m      = 0.0f;           //!< temporary mean-L accumulator

	// Diagonals (reduce directional bias).
	if (sampleMeanL(ctx, cx - dx, cy - dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx + dx, cy - dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx - dx, cy + dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx + dx, cy + dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;

	// Cardinals.
	if (sampleMeanL(ctx, cx - dx, cy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx + dx, cy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx, cy - dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;
	if (sampleMeanL(ctx, cx, cy + dy, offsets.bg, m))
		bgVals[static_cast<std::size_t>(bgCount++)] = m;

	if (bgCount < Params::MIN_BG_SAMPLES) {
		return false;
	}

	std::sort(bgVals.begin(), bgVals.begin() + bgCount);
	const float bgMedian = (bgCount % 2 == 1) ? bgVals[static_cast<std::size_t>(bgCount / 2)]
	                                          : 0.5f * (bgVals[static_cast<std::size_t>(bgCount / 2 - 1)] + bgVals[static_cast<std::size_t>(bgCount / 2)]);

	// 3) Derived features.
	out.deltaL     = innerL - bgMedian;            //!< local-normalized contrast (stone vs background)
	const float da = innerA - Params::LAB_NEUTRAL; //!< a offset from neutral
	const float db = innerB - Params::LAB_NEUTRAL; //!< b offset from neutral
	out.chromaSq   = da * da + db * db;

	// 4) Support fractions inside the inner ROI.
	std::uint32_t total  = 0; //!< ROI pixel count
	std::uint32_t dark   = 0; //!< #pixels with diff <= -SUPPORT_DELTA_L
	std::uint32_t bright = 0; //!< #pixels with diff >= +SUPPORT_DELTA_L
	for (const auto& off: offsets.inner) {
		const int x = cx + off.x; //!< pixel x
		const int y = cy + off.y; //!< pixel y
		if (x < 0 || x >= ctx.cols || y < 0 || y >= ctx.rows) {
			continue;
		}
		const float v    = static_cast<float>(ctx.L.ptr<std::uint8_t>(y)[x]); //!< L at pixel
		const float diff = v - bgMedian;                                      //!< L - bgMedian
		++total;
		if (diff <= -Params::SUPPORT_DELTA_L) {
			++dark;
		} else if (diff >= Params::SUPPORT_DELTA_L) {
			++bright;
		}
	}
	if (total > 0) {
		out.darkFrac   = static_cast<float>(dark) / static_cast<float>(total);
		out.brightFrac = static_cast<float>(bright) / static_cast<float>(total);
	}

	out.valid = true;
	return true;
}

//! Compute features for every intersection point (aligned to geometry.intersections).
static std::vector<Features> computeFeatures(const std::vector<cv::Point2f>& intersections, const SampleContext& ctx, const Offsets& offsets,
                                             const Radii& radii) {
	std::vector<Features> feats(intersections.size());
	for (std::size_t i = 0; i < intersections.size(); ++i) {
		const int cx = static_cast<int>(std::lround(intersections[i].x)); //!< center x (px)
		const int cy = static_cast<int>(std::lround(intersections[i].y)); //!< center y (px)
		Features f{};
		if (computeFeaturesAt(ctx, offsets, radii, cx, cy, f)) {
			feats[i] = f;
		} else {
			feats[i]       = Features{};
			feats[i].valid = false;
		}
	}
	return feats;
}

//! Compute median and robust standard deviation from a sample distribution using MAD.
static bool computeMedianRobustStd(std::vector<float>& samples, float& outMedian, float& outRobustStd) {
	if (samples.empty()) {
		return false;
	}

	std::sort(samples.begin(), samples.end());
	const std::size_t n = samples.size();
	outMedian           = samples[(n - 1) / 2];

	std::vector<float> absDev;
	absDev.reserve(n);
	for (float v: samples) {
		absDev.push_back(std::abs(v - outMedian));
	}
	std::sort(absDev.begin(), absDev.end());
	const float mad = absDev[(n - 1) / 2];

	outRobustStd = std::max(Params::ROBUST_STD_FLOOR, Params::MAD_TO_STD * mad);
	return true;
}

//! Fill Thresholds from a median deltaL and robustStd estimate.
static void fillThresholds(float medianDeltaL, float robustStd, Thresholds& out) {
	const float noiseThresh = Params::K_SIGMA * robustStd;                           //!< noise-based contrast threshold
	const float threshBlack = std::max(Params::MIN_ABS_CONTRAST_BLACK, noiseThresh); //!< black contrast threshold
	const float threshWhite = std::max(Params::MIN_ABS_CONTRAST_WHITE, noiseThresh); //!< white contrast threshold

	out.medianDeltaL   = medianDeltaL;
	out.robustStd      = robustStd;
	out.baseBlack      = medianDeltaL - (threshBlack + Params::BLACK_BIAS);
	out.baseWhite      = medianDeltaL + threshWhite;
	out.neighborMargin = std::max(Params::NEIGHBOR_MARGIN_MIN, Params::NEIGHBOR_MARGIN_STD_MULT * robustStd);
}

//! Compute robust thresholds from the distribution of deltaL values (median + MAD).
static bool computeThresholdsFromMAD(const std::vector<Features>& feats, Thresholds& out) {
	// 1) Collect all deltaL samples from valid intersections.
	std::vector<float> dAll; //!< deltaL distribution over valid intersections
	dAll.reserve(feats.size());
	for (const auto& f: feats) {
		if (f.valid) {
			dAll.push_back(f.deltaL);
		}
	}
	if (dAll.empty()) {
		return false;
	}

	// 2) Initial estimate on all samples (works well early-game, can bias late-game).
	float medianAll    = 0.0f;
	float robustStdAll = 0.0f;
	if (!computeMedianRobustStd(dAll, medianAll, robustStdAll)) {
		return false;
	}
	Thresholds initial{};
	fillThresholds(medianAll, robustStdAll, initial);

	// 3) Second pass: remove strong stone candidates and estimate noise from likely-empty intersections.
	// This reduces late-game bias when the board is heavily populated.
	std::vector<float> dNoise;
	dNoise.reserve(dAll.size());
	for (float v: dAll) {
		if (v > initial.baseBlack && v < initial.baseWhite) {
			dNoise.push_back(v);
		}
	}
	const float removedFrac = 1.0f - (static_cast<float>(dNoise.size()) / static_cast<float>(dAll.size()));
	if (removedFrac < Params::NOISE_REESTIMATE_REMOVED_FRAC_MIN) {
		out = initial;
		return true;
	}
	if (dNoise.size() < static_cast<std::size_t>(Params::MIN_NOISE_SAMPLES)) {
		out = initial;
		return true;
	}

	float medianNoise    = 0.0f;
	float robustStdNoise = 0.0f;
	if (!computeMedianRobustStd(dNoise, medianNoise, robustStdNoise)) {
		out = initial;
		return true;
	}

	fillThresholds(medianNoise, robustStdNoise, out);
	return true;
}

//! Median deltaL of the 8-neighborhood (valid neighbors only). Falls back to \p fallback if no neighbors exist.
static float neighborMedianDelta(const std::vector<Features>& feats, int N, int idx, float fallback) {
	if (N <= 0 || idx < 0 || static_cast<std::size_t>(idx) >= feats.size()) {
		return fallback;
	}

	const int x = idx / N;     //!< grid x-index
	const int y = idx - x * N; //!< grid y-index

	std::array<float, 8> vals{}; //!< neighbor deltaL values (up to 8)
	int cnt = 0;                 //!< number of valid neighbors
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			if (dx == 0 && dy == 0) {
				continue; // skip self
			}
			const int nx = x + dx; //!< neighbor x
			const int ny = y + dy; //!< neighbor y
			if (nx < 0 || nx >= N || ny < 0 || ny >= N) {
				continue;
			}
			const int nidx = nx * N + ny; //!< neighbor flattened index
			if (nidx < 0 || static_cast<std::size_t>(nidx) >= feats.size()) {
				continue;
			}
			if (!feats[static_cast<std::size_t>(nidx)].valid) {
				continue;
			}
			vals[static_cast<std::size_t>(cnt++)] = feats[static_cast<std::size_t>(nidx)].deltaL;
		}
	}
	if (cnt == 0) {
		return fallback;
	}
	std::sort(vals.begin(), vals.begin() + cnt);
	return (cnt % 2 == 1) ? vals[static_cast<std::size_t>(cnt / 2)]
	                      : 0.5f * (vals[static_cast<std::size_t>(cnt / 2 - 1)] + vals[static_cast<std::size_t>(cnt / 2)]);
}

/*! Coarse candidate detection from local-normalized contrast (deltaL).
 * \return Candidate{Empty,0} if not beyond coarse black/white thresholds.
 */
static Candidate classifyCoarse(const Features& f, const Thresholds& thr) {
	Candidate cand{};
	if (!f.valid) {
		return cand;
	}

	if (f.deltaL <= thr.baseBlack) {
		cand.type     = StoneState::Black;
		cand.strength = thr.baseBlack - f.deltaL;
	} else if (f.deltaL >= thr.baseWhite) {
		cand.type     = StoneState::White;
		cand.strength = f.deltaL - thr.baseWhite;
	}
	return cand;
}

//! Edge behavior depends on the grid location; centralize the mapping here (no scattered onEdge/nearEdge checks).
static EdgeProfile getEdgeProfile(int gx, int gy, int N) {
	if (N <= 0) {
		return {};
	}

	const bool onEdge   = (gx == 0) || (gx == N - 1) || (gy == 0) || (gy == N - 1); //!< outermost ring
	const bool nearEdge = (gx <= 1) || (gx >= N - 2) || (gy <= 1) || (gy >= N - 2); //!< 2-wide border

	if (onEdge) {
		return {Params::EDGE_CHROMA_SQ_CAP_ON_EDGE, Params::EDGE_WEAK_MARGIN, Params::REFINE_RANGE_ON_EDGE, Params::REFINE_EXTENT_ON_EDGE,
		        Params::REFINE_MIN_PRE_BF_EDGE};
	}
	if (nearEdge) {
		return {Params::EDGE_CHROMA_SQ_CAP_NEAR_EDGE, Params::EDGE_WEAK_MARGIN, Params::REFINE_RANGE_INTERIOR, Params::REFINE_EXTENT_NEAR,
		        Params::REFINE_MIN_PRE_BF};
	}
	return {Params::MAX_CHROMA_SQ_WHITE, 0.0f, Params::REFINE_RANGE_INTERIOR, Params::REFINE_EXTENT, Params::REFINE_MIN_PRE_BF};
}

//! Hard chroma reject (fast path): rejects strongly colored artifacts regardless of stone type.
static bool passesHardChromaCap(const Features& f, const ChromaProfile& chroma) {
	return f.chromaSq <= chroma.hardCap;
}

//! Chroma gating (reject non-neutral artifacts). Includes a global hard cap.
static bool passesChromaGate(const Features& f, const Candidate& cand, const EdgeProfile& edge, const ChromaProfile& chroma) {
	if (!passesHardChromaCap(f, chroma)) {
		return false;
	}
	if (cand.type == StoneState::Empty) {
		return true;
	}

	if (cand.type == StoneState::Black) {
		return f.chromaSq <= chroma.blackCap;
	}

	// White stones: stricter chroma near edges and for weak-contrast candidates.
	if (f.chromaSq > chroma.whiteCap) {
		return false;
	}
	if (f.chromaSq > edge.chromaCap) {
		return false;
	}
	if (cand.strength < Params::WEAK_WHITE_MARGIN && f.chromaSq > chroma.strictWhiteCap) {
		return false;
	}
	if ((edge.weakMargin > 0.0f) && (cand.strength < edge.weakMargin) && (f.chromaSq > Params::EDGE_MAX_CHROMA_SQ_WEAK)) {
		return false;
	}
	return true;
}

//! Support gating based on the fraction of ROI pixels that agree with the hypothesis.
static bool passesSupportGate(const Features& f, StoneState candidate) {
	if (candidate == StoneState::Black) {
		if (f.darkFrac < Params::MIN_SUPPORT_FRAC_BLACK) {
			return false;
		}
		return f.darkFrac >= f.brightFrac + Params::SUPPORT_DOMINANCE_MARGIN;
	}
	if (candidate == StoneState::White) {
		if (f.brightFrac < Params::MIN_SUPPORT_FRAC_WHITE) {
			return false;
		}
		return f.brightFrac >= f.darkFrac + Params::SUPPORT_DOMINANCE_MARGIN;
	}
	return false;
}

//! Neighbor gating: require a strong local contrast vs the 8-neighborhood median.
static bool passesNeighborGate(const Features& f, float neighborMedian, const Thresholds& thr, StoneState candidate) {
	if (candidate == StoneState::Black) {
		return f.deltaL <= neighborMedian - thr.neighborMargin;
	}
	if (candidate == StoneState::White) {
		return f.deltaL >= neighborMedian + thr.neighborMargin;
	}
	return false;
}

//! Confidence (in "sigmas") of a coarse Black/White hypothesis.
static float computeConfidence(const Candidate& cand, float robustStd) {
	if (cand.type == StoneState::Empty) {
		return 0.0f;
	}
	if (!std::isfinite(robustStd) || robustStd <= 0.0f) {
		return 0.0f;
	}
	const float c = cand.strength / robustStd;
	return std::clamp(c, 0.0f, Params::CONFIDENCE_MAX);
}

/*! Temporal stabilization hook (future extension point).
 *
 * Intention: combine the current per-intersection classification with the previous frame (and confidence)
 * to reduce flicker. This is intentionally a no-op for now and is not wired into analyseBoard().
 */
[[maybe_unused]] static StoneState stabilizeWithPreviousFrame(std::size_t /*idx*/, StoneState raw, float /*confidence*/, StoneState /*previous*/,
                                                              float /*previousConfidence*/) {
	return raw;
}

/*! Optional refinement for white stones by searching a small pixel neighborhood.
 *
 * Whites can be low-contrast; small grid/intersection jitter can move the ROI off-center and reduce support.
 * We search a small window and keep the best-supported white hypothesis (highest deltaL).
 */
static bool shouldAttemptWhiteRefinement(const Features& current, const Thresholds& thr, const EdgeProfile& edge, StoneState coarseType) {
	const bool candWhite = (coarseType == StoneState::White);

	const float extraRange =
	        (edge.refineRange == Params::REFINE_RANGE_INTERIOR && (current.brightFrac >= Params::REFINE_EXTRA_RANGE_BF)) ? Params::REFINE_EXTRA_RANGE : 0.0f;

	const bool inActivationBand = std::abs(current.deltaL - thr.baseWhite) <= Params::REFINE_ACTIVATION_BAND;
	const bool needsSupport =
	        candWhite && inActivationBand &&
	        ((current.brightFrac < Params::MIN_SUPPORT_FRAC_WHITE) || (current.brightFrac < current.darkFrac + Params::SUPPORT_DOMINANCE_MARGIN));

	const bool borderline = (!candWhite && (current.deltaL >= thr.baseWhite - (edge.refineRange + extraRange)) && (current.brightFrac >= edge.minPreBright) &&
	                         (current.chromaSq <= Params::BORDERLINE_MAX_CHROMA_SQ));

	return needsSupport || borderline;
}

//! White refinement candidate gating (no neighbor gate; we only re-center the ROI for a stronger white hypothesis).
static bool isRefinedWhiteCandidate(const Features& f, const Thresholds& thr, const EdgeProfile& edge, const ChromaProfile& chroma) {
	const Candidate cand = classifyCoarse(f, thr);
	if (cand.type != StoneState::White) {
		return false;
	}

	// Use the same chroma + support gating as the final stage; refinement should re-center, not relax acceptance.
	return passesChromaGate(f, cand, edge, chroma) && passesSupportGate(f, StoneState::White);
}

//! Search a small neighborhood around the intersection and return the best refined white Features (max deltaL).
static std::optional<Features> searchBestRefinedWhite(const cv::Point2f& intersection, const SampleContext& ctx, const Offsets& offsets, const Radii& radii,
                                                      const Thresholds& thr, const EdgeProfile& edge, const ChromaProfile& chroma) {
	const int cx0    = static_cast<int>(std::lround(intersection.x)); //!< initial center x
	const int cy0    = static_cast<int>(std::lround(intersection.y)); //!< initial center y
	const int extent = edge.refineExtent;

	bool found = false; //!< whether we found a refined white candidate
	Features best{};    //!< best refined Features

	for (int oy = -extent; oy <= extent; oy += Params::REFINE_STEP_PX) {
		for (int ox = -extent; ox <= extent; ox += Params::REFINE_STEP_PX) {
			if (ox == 0 && oy == 0) {
				continue;
			}
			Features f{};
			if (!computeFeaturesAt(ctx, offsets, radii, cx0 + ox, cy0 + oy, f)) {
				continue;
			}
			if (!isRefinedWhiteCandidate(f, thr, edge, chroma)) {
				continue;
			}
			if (!found || f.deltaL > best.deltaL) {
				found = true;
				best  = f;
			}
		}
	}

	if (!found) {
		return std::nullopt;
	}
	return best;
}

static std::optional<Features> refineWhiteIfNeeded(const cv::Point2f& intersection, const SampleContext& ctx, const Offsets& offsets, const Radii& radii,
                                                   const Features& current, const Thresholds& thr, const EdgeProfile& edge, const ChromaProfile& chroma,
                                                   StoneState coarseType, DetectionStats* stats) {
	if (coarseType == StoneState::Black) {
		return std::nullopt;
	}
	if (!shouldAttemptWhiteRefinement(current, thr, edge, coarseType)) {
		return std::nullopt;
	}
	if (stats) {
		++stats->refinementTriggered;
	}
	if (auto refined = searchBestRefinedWhite(intersection, ctx, offsets, radii, thr, edge, chroma)) {
		if (stats) {
			++stats->refinementAccepted;
		}
		return refined;
	}
	return std::nullopt;
}

//! Classify all intersections and fill an aligned state vector (size = intersections.size()).
static void classifyAll(const std::vector<cv::Point2f>& intersections, const SampleContext& ctx, const Offsets& offsets, const Radii& radii,
                        const std::vector<Features>& feats, const Thresholds& thr, unsigned boardSize, std::vector<StoneState>& outStates,
                        std::vector<float>& outConfidence, DetectionStats* stats) {
	const int N = static_cast<int>(boardSize); //!< board size (grid is N x N)
	constexpr ChromaProfile chroma{};          //!< consolidated chroma thresholds
	outStates.assign(intersections.size(), StoneState::Empty);
	outConfidence.assign(intersections.size(), 0.0f);
	if (stats) {
		*stats = DetectionStats{};
	}

	for (std::size_t i = 0; i < intersections.size(); ++i) { // i = flattened intersection index
		const Features& base = feats[i];                     //!< unrefined feature snapshot
		if (!base.valid) {
			continue;
		}
		if (stats) {
			++stats->total;
		}

		const int gx           = (N > 0) ? (static_cast<int>(i) / N) : 0;      //!< grid x-index
		const int gy           = (N > 0) ? (static_cast<int>(i) - gx * N) : 0; //!< grid y-index
		const EdgeProfile edge = getEdgeProfile(gx, gy, N);                    //!< edge-specific thresholds at this location

		// 1) Global hard cap: reject strongly colored artifacts early.
		if (!passesHardChromaCap(base, chroma)) {
			if (stats) {
				++stats->rejectedChroma;
			}
			continue;
		}

		// 2) Coarse candidate from deltaL thresholds.
		Features f     = base;
		Candidate cand = classifyCoarse(f, thr);

		// 3) Optional refinement for white (or borderline) hypotheses.
		if (cand.type != StoneState::Black) {
			if (const auto refined = refineWhiteIfNeeded(intersections[i], ctx, offsets, radii, f, thr, edge, chroma, cand.type, stats)) {
				f    = *refined;
				cand = classifyCoarse(f, thr);
			}
		}

		// 4) Reject non-candidates quickly.
		if (cand.type == StoneState::Empty) {
			continue;
		}

		// 5) Local contrast gating vs neighbor median.
		const float neigh = neighborMedianDelta(feats, N, static_cast<int>(i), thr.medianDeltaL); //!< local deltaL baseline

		if (!passesNeighborGate(f, neigh, thr, cand.type)) {
			if (stats) {
				++stats->rejectedNeighbor;
			}
			continue;
		}

		// 6) Candidate-specific chroma + support gating.
		if (!passesChromaGate(f, cand, edge, chroma)) {
			if (stats) {
				++stats->rejectedChroma;
			}
			continue;
		}
		if (!passesSupportGate(f, cand.type)) {
			if (stats) {
				++stats->rejectedSupport;
			}
			continue;
		}

		outStates[i]     = cand.type;
		outConfidence[i] = computeConfidence(cand, thr.robustStd);
		if (stats) {
			stats->classifiedBlack += (cand.type == StoneState::Black) ? 1 : 0;
			stats->classifiedWhite += (cand.type == StoneState::White) ? 1 : 0;
		}
	}
}

//! Draw a visualization overlay (black stones in black, white stones in blue).
static cv::Mat drawOverlay(const cv::Mat& boardImage, const std::vector<cv::Point2f>& intersections, const std::vector<StoneState>& states, int radius) {
	cv::Mat vis = boardImage.clone();
	for (std::size_t i = 0; i < intersections.size() && i < states.size(); ++i) {
		if (states[i] == StoneState::Black) {
			cv::circle(vis, intersections[i], radius, cv::Scalar(0, 0, 0), 2);
		} else if (states[i] == StoneState::White) {
			cv::circle(vis, intersections[i], radius, cv::Scalar(255, 0, 0), 2);
		}
	}
	return vis;
}

//! Render the DetectionStats counters into a small image tile for the DebugVisualizer mosaic.
static cv::Mat renderStatsTile(const DetectionStats& s) {
	cv::Mat tile(220, 420, CV_8UC3, cv::Scalar(255, 255, 255));

	int y          = 24;
	const auto put = [&](const std::string& line) {
		cv::putText(tile, line, cv::Point(10, y), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
		y += 22;
	};

	put("Stone Detection Stats");
	put("total: " + std::to_string(s.total));
	put("black: " + std::to_string(s.classifiedBlack));
	put("white: " + std::to_string(s.classifiedWhite));
	put("rej(chroma): " + std::to_string(s.rejectedChroma));
	put("rej(support): " + std::to_string(s.rejectedSupport));
	put("rej(neighbor): " + std::to_string(s.rejectedNeighbor));
	put("refine(trig): " + std::to_string(s.refinementTriggered));
	put("refine(acc): " + std::to_string(s.refinementAccepted));
	return tile;
}

} // namespace

/*! Detect stones on a rectified Go board.
 *
 * Major steps:
 *  1) Validate inputs and start a debug stage.
 *  2) Choose radii and precompute ROI offsets from the estimated grid spacing.
 *  3) Convert to Lab and blur channels to suppress thin grid lines.
 *  4) Compute per-intersection features (deltaL/chroma/support fractions).
 *  5) Compute robust thresholds from the deltaL distribution (median + MAD).
 *  6) Classify all intersections into Empty/Black/White (optional refinement for whites).
 *  7) Optionally draw a visualization overlay.
 *
 * \param [in]     geometry Rectified board image + intersection points (in rectified coordinates).
 * \param [in,out] debugger Optional visualizer (may be nullptr). When present, an overlay is drawn.
 * \return         StoneResult with success flag and a stone-state vector aligned to geometry.intersections.
 */
StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger) {
	// 0. Validate inputs.
	if (geometry.image.empty()) {
		std::cerr << "Failed to rectify image\n";
		return {false, {}, {}};
	}
	if (geometry.intersections.empty()) {
		std::cerr << "No intersections provided\n";
		return {false, {}, {}};
	}
	if (geometry.boardSize == 0u || geometry.intersections.size() != geometry.boardSize * geometry.boardSize) {
		std::cerr << "Invalid board geometry\n";
		return {false, {}, {}};
	}

	// 1. Optional debug stage begin.
	if (debugger) {
		debugger->beginStage("Stone Detection");
		debugger->add("Input", geometry.image);
	}

	// 2. Choose radii/distances and precompute ROI offsets.
	const Radii radii     = chooseRadii(geometry.spacing);
	const Offsets offsets = precomputeOffsets(radii);

	// 3. Prepare blurred Lab channels for fast sampling.
	LabBlur blur{};
	if (!prepareLabBlur(geometry.image, radii, blur)) {
		std::cerr << "Unsupported image channels: " << geometry.image.channels() << "\n";
		if (debugger) {
			debugger->endStage();
		}
		return {false, {}, {}};
	}
	const SampleContext ctx{blur.L, blur.A, blur.B, blur.L.rows, blur.L.cols};

	// 4. Compute features per intersection (aligned to geometry.intersections).
	const std::vector<Features> feats = computeFeatures(geometry.intersections, ctx, offsets, radii);

	// 5. Compute robust thresholds from the deltaL distribution.
	Thresholds thr{};
	if (!computeThresholdsFromMAD(feats, thr)) {
		if (debugger) {
			debugger->endStage();
		}
		return {false, {}, {}};
	}

	// 6. Classify each intersection into Empty/Black/White.
	std::vector<StoneState> states; //!< stone states aligned to intersections
	std::vector<float> confidence;  //!< per-intersection confidence aligned to intersections
	DetectionStats stats{};         //!< optional counters for debugging/tuning
	classifyAll(geometry.intersections, ctx, offsets, radii, feats, thr, geometry.boardSize, states, confidence, debugger ? &stats : nullptr);

	// 7. Optional visualization overlay.
	if (debugger) {
		const cv::Mat vis = drawOverlay(geometry.image, geometry.intersections, states, radii.innerRadius);
		debugger->add("Stone Overlay", vis);
		debugger->add("Stone Stats", renderStatsTile(stats));
		debugger->endStage();
	}

	return {true, std::move(states), std::move(confidence)};
}

} // namespace go::camera
