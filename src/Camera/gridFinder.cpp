#include "gridFinder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>

/**
 * @brief 1D lattice fitting for Go board grid lines.
 *
 * Input to this module is a *sorted* list of candidate line-center positions per axis (pixels).
 * Detections may have missing lines and may include spurious lines (most commonly the physical board border).
 *
 * We model the grid as a 1D periodic lattice and fit it deterministically:
 *  1) estimate spacing from the mode of adjacent gaps,
 *  2) estimate phase by clustering modulo residuals r_i = c_i mod s,
 *  3) enumerate integer offsets that map detections to lattice indices,
 *  4) score fits by explained structure (inliers/span) and RMS alignment error,
 *  5) choose N jointly across vertical+horizontal fits and reconstruct equally-spaced grids.
 *
 * This avoids tolerance-based snapping loops and naturally rejects border artifacts as outliers.
 */
namespace go::camera {

/*! Estimate the dominant adjacent gap (grid spacing) using a coarse histogram.
 *  Using a mode (instead of a mean) is fast and robust against outlier gaps introduced by missing/spurious lines.
 *
 * \param [in] gaps     Adjacent differences between sorted candidate centers (pixels).
 * \param [in] binWidth Histogram bin width (pixels).
 * \return     Estimated spacing in pixels, or 0.0 if @p gaps is empty.
 */
double modeGap(const std::vector<double>& gaps, double binWidth) {
	// 0. Guard.
	if (gaps.empty())
		return 0.0;

	// 1. Determine the observed gap range.
	const double gmin = *std::min_element(gaps.begin(), gaps.end()); //!< Min gap (px)
	const double gmax = *std::max_element(gaps.begin(), gaps.end()); //!< Max gap (px)

	// 2. Histogram gaps into uniform bins.
	const int bins = (int)std::ceil((gmax - gmin) / binWidth) + 1; //!< Number of bins (>= 1)
	std::vector<int> hist(bins, 0);                                //!< Per-bin counts

	// g = one adjacent gap (px)
	for (double g: gaps) {
		const int b = (int)std::floor((g - gmin) / binWidth); //!< Bin index
		if (b >= 0 && b < bins)
			hist[b]++; // vote
	}

	// 3. Pick the most populated bin and return its center.
	const int bestBin = (int)(std::max_element(hist.begin(), hist.end()) - hist.begin()); //!< argmax(hist)
	return gmin + (bestBin + 0.5) * binWidth;                                             // bin center (px)
}

/*! Positive modulo helper in [0, period).
 * \param [in] x      Value to wrap.
 * \param [in] period Period (must be > 0).
 * \return Wrapped value in [0, period), or 0.0 if @p period <= 0.
 */
static double positiveFmod(const double x, const double period) {
	if (period <= 0.0) {
		return 0.0;
	}

	double r = std::fmod(x, period); // raw fmod result
	if (r < 0.0) {
		r += period; // shift into [0, period)
	}
	if (r >= period) {
		r = 0.0; // guard against rare numerical edge cases
	}
	return r;
}

/*! Estimate the dominant lattice phase (offset) given a spacing.
 * For a true lattice with spacing s, centers c_i cluster around a common residual in:
 *   r_i = c_i mod s
 * We histogram residuals to find the dominant cluster and then compute a circular mean within that cluster neighborhood for sub-bin precision (handles
 * wrap-around at 0/s).
 *
 * \param [in] centersSorted Sorted candidate center positions (pixels).
 * \param [in] spacing       Lattice spacing (pixels).
 * \return     Phase r* in [0, spacing).
 */
static double dominantResidualPhase(const std::vector<double>& centersSorted, const double spacing) {
	if (centersSorted.empty() || spacing <= 0.) {
		return 0.;
	}

	// 1. Compute residuals r_i = c_i mod spacing, wrapped to [0, spacing).
	std::vector<double> residuals; // residuals in [0, spacing)
	residuals.reserve(centersSorted.size());
	for (auto c: centersSorted) { // c = candidate center (px)
		residuals.push_back(positiveFmod(c, spacing));
	}

	// 2. Histogram residuals to find the dominant phase cluster.
	const double binWidth = std::clamp(0.04 * spacing, 1.0, 4.0);                         //!< Px (few pixels)
	const int bins        = std::max(8, static_cast<int>(std::ceil(spacing / binWidth))); //!< Number of bins around [0, spacing)
	std::vector<int> hist(static_cast<std::size_t>(bins), 0);                             //!< Residual histogram

	for (double r: residuals) {
		int b = static_cast<int>(std::floor(r / binWidth));   //!< Bin index
		b     = std::clamp(b, 0, static_cast<int>(bins) - 1); // Safety clamp
		hist[static_cast<std::size_t>(b)]++;                  // Vote
	}

	//! \param [in] i Histogram bin index
	auto smoothedCount = [&](const int i) -> int {
		const int prev = (i - 1 + bins) % bins;                                                                                 //!< Wrap-around previous
		const int next = (i + 1) % bins;                                                                                        //!< Wrap-around next
		return hist[static_cast<std::size_t>(prev)] + hist[static_cast<std::size_t>(i)] + hist[static_cast<std::size_t>(next)]; // 3-bin smoothing
	};

	// 3. Pick the (smoothed) mode bin.
	int bestBin   = 0;                  // best bin index
	int bestCount = -1;                 // best smoothed count
	for (int i = 0; i < bins; ++i) {    // i = bin index
		const int c = smoothedCount(i); // smoothed votes
		if (c > bestCount) {
			bestCount = c;
			bestBin   = i;
		}
	}

	const int prev = (bestBin - 1 + bins) % bins; //!< Neighborhood bin
	const int next = (bestBin + 1) % bins;        //!< Neighborhood bin

	// 4. Compute circular mean inside the dominant bin window (bestBin +/- 1) to get sub-bin phase.
	double sumSin = 0.0; // sum of sin(angle)
	double sumCos = 0.0; // sum of cos(angle)
	int used      = 0;   // number of residuals used

	//! r = residual (px)
	for (double r: residuals) {
		int b = static_cast<int>(std::floor(r / binWidth)); //!< Bin index
		b     = std::clamp(b, 0, bins - 1);                 // safety clamp
		if (b != bestBin && b != prev && b != next) {
			continue; // keep dominant cluster only
		}

		const double ang = 2. * std::numbers::pi * (r / spacing); // map residual to a circle angle
		sumSin += std::sin(ang);
		sumCos += std::cos(ang);
		++used;
	}

	if (used == 0 || (std::abs(sumSin) + std::abs(sumCos)) < 1e-12) {
		// Fallback: center of dominant bin (still deterministic).
		const double phase = (static_cast<double>(bestBin) + 0.5) * binWidth; // bin center (px)
		return std::clamp(phase, 0.0, std::nextafter(spacing, 0.0));
	}

	// 5. Convert circular mean back to a phase in pixels.
	double ang = std::atan2(sumSin, sumCos); // mean angle in [-pi, pi]
	if (ang < 0.0) {
		ang += 2. * std::numbers::pi;
	}
	const double phase = (ang / (2. * std::numbers::pi)) * spacing; // phase r* (px)
	return std::clamp(phase, 0.0, std::nextafter(spacing, 0.0));
}

struct LatticeScore {
	double rms{std::numeric_limits<double>::infinity()}; //!< RMS alignment error (px) over inlier indices
	std::size_t inliers{0};                              //!< Number of lattice indices k that received a match
	int span{0};                                         //!< Contiguous index span covered by inliers
	int offset{0};                                       //!< Integer multiple of spacing applied to the phase
};

/*! Score a specific lattice start (phase + offset) for a given spacing and N.
 *  Each detected center is assigned to its nearest lattice index:
 *    k = round((c - start) / spacing)
 *  For each k we keep only the best-matching detection (smallest |error|).
 *  The score is computed from indices that got a match (inliers), their covered span, and the RMS alignment error.
 * \param [in]  centersSorted Sorted candidate center positions (pixels).
 * \param [in]  start         Lattice start position grid[0] (pixels).
 * \param [in]  spacing       Lattice spacing (pixels).
 * \param [in]  N             Board size (number of lines on this axis).
 * \param [out] outScore      Filled with inlier/span/RMS metrics if a valid score can be computed.
 * \return      True if at least one inlier was found; false otherwise.
 */
static bool evaluateLatticeOffset(const std::vector<double>& centersSorted, double start, double spacing, int N, LatticeScore& outScore) {
	// 0. Guard.
	if (N <= 0 || spacing <= 0.0)
		return false;

	// 1. For each lattice index k, keep the closest detected center (best absolute residual).
	std::vector<double> bestErr(static_cast<std::size_t>(N), std::numeric_limits<double>::infinity()); // per-k best error (px)
	std::vector<uint8_t> has(static_cast<std::size_t>(N), 0u);                                         // per-k match flag

	for (double c: centersSorted) {                                // c = detected center (px)
		const double kReal = (c - start) / spacing;                // real-valued index
		const int k        = static_cast<int>(std::lround(kReal)); // nearest lattice index
		if (k < 0 || k >= N)
			continue;

		const double predicted = start + static_cast<double>(k) * spacing; // lattice position for k
		const double e         = c - predicted;                            // residual error (px)
		const double ae        = std::abs(e);                              // |residual| (px)

		const std::size_t ki = static_cast<std::size_t>(k); // k as size_t for indexing
		if (has[ki] == 0u || ae < std::abs(bestErr[ki])) {
			bestErr[ki] = e;
			has[ki]     = 1u;
		}
	}

	// 2. Compute inlier count, covered span, and RMS error.
	double sumSq     = 0.0;       // sum of squared errors
	unsigned inliers = 0u;        // number of matched indices
	int minK         = N;         // smallest matched k
	int maxK         = -1;        // largest matched k
	for (int k = 0; k < N; ++k) { // k = lattice index
		const std::size_t ki = static_cast<std::size_t>(k);
		if (has[ki] == 0u)
			continue;
		++inliers;
		sumSq += bestErr[ki] * bestErr[ki];
		minK = std::min(minK, k);
		maxK = std::max(maxK, k);
	}

	if (inliers == 0u)
		return false;

	// 3. Fill output score.
	outScore.rms     = std::sqrt(sumSq / static_cast<double>(inliers));
	outScore.inliers = inliers;
	outScore.span    = (maxK >= minK) ? (maxK - minK + 1) : 0;
	return true;
}

/*!
 * Fit a 1D equally-spaced lattice to candidate centers for a fixed N.
 * Major steps:
 *  1) (Optional) slide a contiguous window of size N over detections to reduce edge outlier influence,
 *  2) estimate spacing from the modal adjacent gap in the window (modeGap()),
 *  3) estimate phase by clustering modulo residuals (dominantResidualPhase()),
 *  4) enumerate integer offsets (indexing) and score each start by inliers/span + RMS,
 *  5) select the best-scoring start and reconstruct the lattice.
 *
 * \param [in]  centersSorted Sorted candidate center positions for one axis (pixels).
 * \param [in]  N             Board size to fit (number of lines on this axis).
 * \param [out] outGrid       Selected lattice positions (size = N).
 * \return      True if a lattice fit was found; false otherwise.
 */
static bool selectGridByLatticeFit(const std::vector<double>& centersSorted, const std::size_t N, std::vector<double>& outGrid) {
	outGrid.clear();

	// Need enough detections to estimate spacing/phase robustly.
	if (centersSorted.size() < 6) {
		return false;
	}

	// Global mode spacing from the full axis candidates (used as a consistency prior across variants).
	std::vector<double> fullGaps;
	fullGaps.reserve(centersSorted.size() - 1);
	for (std::size_t i = 0; i + 1 < centersSorted.size(); ++i) {
		fullGaps.push_back(centersSorted[i + 1] - centersSorted[i]);
	}
	const double globalModeSpacing = modeGap(fullGaps, 4.0);

	// 1. Track the best fit for this N.
	LatticeScore bestForN{};                                                   //!< Best score for this N
	double bestStartForN            = 0.0;                                     //!< Best start for this N
	double bestSpacingForN          = 0.0;                                     //!< Best spacing for this N
	double bestPhaseForN            = 0.0;                                     //!< Best phase for this N
	std::size_t bestWindowStartForN = 0;                                       //!< Best window start for this N
	double bestGapRmsForN           = std::numeric_limits<double>::infinity(); //!< Best window gapRms for this N

	// 2. Build evaluation variants.
	//    - Standard contiguous windows of size N (or M if M < N).
	//    - When M == N, also evaluate "drop-one" variants (size N-1) so one spurious border line can be
	//      removed and the missing true line extrapolated by the lattice fit.
	const std::size_t M          = centersSorted.size();   // number of detections
	const std::size_t windowSize = M >= N ? N : M;         //!< Window size
	const std::size_t windows    = M >= N ? M - N + 1 : 1; //!< Number of contiguous windows
	const bool allowDropOne      = (M == N);               //!< Evaluate subsets with one removed detection
	const std::size_t variants   = windows + (allowDropOne ? M : 0);

	// 3. Evaluate each variant for this N.
	for (std::size_t variant = 0; variant < variants; ++variant) {
		std::vector<double> centersWindow;
		centersWindow.reserve(windowSize);
		std::size_t wStart = 0; // deterministic tie-break proxy for this variant

		if (variant < windows) {
			wStart             = (windows == 1) ? 0 : variant;
			const auto beginIt = centersSorted.begin() + static_cast<std::ptrdiff_t>(wStart);
			const auto endIt   = beginIt + static_cast<std::ptrdiff_t>(windowSize);
			centersWindow.assign(beginIt, endIt);
		} else {
			const std::size_t drop = variant - windows; // dropped detection index
			centersWindow.clear();
			centersWindow.reserve(M - 1);
			for (std::size_t i = 0; i < M; ++i) {
				if (i == drop) {
					continue;
				}
				centersWindow.push_back(centersSorted[i]);
			}
			wStart = drop;
		}

		if (centersWindow.size() < 2)
			continue;

		// 3.1 Estimate spacing from adjacent gaps (windowed to reduce influence of border outliers).
		std::vector<double> gaps; //!< Adjacent gaps (px)
		gaps.reserve(centersWindow.size() - 1);
		for (std::size_t i = 0; i < centersWindow.size() - 1; ++i) {
			gaps.push_back(centersWindow[i + 1] - centersWindow[i]);
		}

		const double spacing = modeGap(gaps, 4.0); // spacing estimate (px)
		if (spacing < 1e-6 || !std::isfinite(spacing)) {
			continue;
		}

		// 3.2 Gap regularity (tie-break within this N): true grid lines have near-constant adjacent gaps.
		double gapSumSq = 0.0; //!< sum of squared gap residuals
		for (auto g: gaps) {
			const double d = g - spacing;
			gapSumSq += d * d;
		}
		const double gapRms = gaps.empty() ? std::numeric_limits<double>::infinity() : std::sqrt(gapSumSq / static_cast<double>(gaps.size()));

		// 3.3 Estimate phase from modulo residual clustering.
		const double phase = dominantResidualPhase(centersWindow, spacing); // phase r* (px)

		// 3.4 Enumerate integer offsets: start = phase + offset * spacing.
		//     Offsets come from mapping detections to approximate lattice indices.
		std::vector<int> kApprox; // approx index per detection
		kApprox.reserve(centersWindow.size());
		// c = detected center (px)
		for (double c: centersWindow) {
			kApprox.push_back(static_cast<int>(std::lround((c - phase) / spacing)));
		}

		std::vector<int> offsets; // unique offsets to test
		offsets.reserve(kApprox.size() * static_cast<std::size_t>(N));
		// k = approximate lattice index for one detection
		for (int k: kApprox) {
			// j = candidate lattice index [0..N-1]
			for (int j = 0; j < static_cast<int>(N); ++j) {
				offsets.push_back(k - j);
			}
		}

		std::sort(offsets.begin(), offsets.end());
		offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());

		// 3.5 Evaluate each offset and keep the best for this window.
		LatticeScore bestForWindow{};    //!< Best score for this window
		double bestStartForWindow = 0.0; //!< Best lattice start for this window

		// offset = integer shift applied to phase
		for (int offset: offsets) {
			const double start = phase + static_cast<double>(offset) * spacing; //!< candidate grid[0] (px)

			LatticeScore score{}; //!< score for this candidate start
			score.offset = offset;
			if (!evaluateLatticeOffset(centersWindow, start, spacing, N, score))
				continue;

			// Selection criteria for this window:
			// maximize explained structure first (inliers/span), then minimize alignment error.
			static constexpr double RMS_EPS = 1e-6;                                                    //!< Epsilon for RMS comparisons (numerical stability)
			const bool betterInliers        = score.inliers > bestForWindow.inliers;                   //!< Prefer more matched lattice indices
			const bool equalInliers         = score.inliers == bestForWindow.inliers;                  //!< Tie on inliers
			const bool betterSpan           = score.span > bestForWindow.span;                         //!< Prefer larger covered index span
			const bool equalSpan            = score.span == bestForWindow.span;                        //!< Tie on span
			const bool betterRms            = score.rms + RMS_EPS < bestForWindow.rms;                 //!< Prefer smaller RMS alignment error
			const bool equalRms             = std::abs(score.rms - bestForWindow.rms) <= RMS_EPS;      //!< Tie on RMS
			const bool betterOffset         = std::abs(score.offset) < std::abs(bestForWindow.offset); //!< Prefer smaller |offset| (more centered indexing)

			if (betterInliers || (equalInliers && (betterSpan || (equalSpan && (betterRms || (equalRms && betterOffset)))))) {
				bestForWindow      = score;
				bestStartForWindow = start;
			}
		}

		if (!std::isfinite(bestForWindow.rms) || bestForWindow.inliers == 0) {
			continue;
		}

		// 3.6 Select the best variant for this N using a composite objective:
		//     objective = rms + lambdaGap * gapRms + lambdaMiss * missingLines + lambdaSpacing * |spacing - globalModeSpacing|.
		// The spacing consistency term stabilizes selection when drop-one variants can overfit a subset.
		static constexpr double RMS_EPS  = 1e-6; //!< Epsilon for floating comparisons
		static constexpr double LAMBDA_G = 0.35; //!< Weight for adjacent-gap irregularity
		static constexpr double LAMBDA_M = 2.00; //!< Weight per missing lattice line (N - inliers)
		static constexpr double LAMBDA_S = 1.50; //!< Weight for spacing deviation from global mode spacing

		const double missCur        = static_cast<double>(N) - static_cast<double>(bestForWindow.inliers);
		const double missBest       = static_cast<double>(N) - static_cast<double>(bestForN.inliers);
		const double spacingDevCur  = std::isfinite(globalModeSpacing) ? std::abs(spacing - globalModeSpacing) : 0.0;
		const double spacingDevBest = std::isfinite(globalModeSpacing) ? std::abs(bestSpacingForN - globalModeSpacing) : 0.0;
		const double objectiveCur   = bestForWindow.rms + LAMBDA_G * gapRms + LAMBDA_M * missCur + LAMBDA_S * spacingDevCur;
		const double objectiveBest  = bestForN.rms + LAMBDA_G * bestGapRmsForN + LAMBDA_M * missBest + LAMBDA_S * spacingDevBest;

		const bool betterObjective  = objectiveCur + RMS_EPS < objectiveBest;
		const bool equalObjective   = std::abs(objectiveCur - objectiveBest) <= RMS_EPS;
		const bool betterInliers    = bestForWindow.inliers > bestForN.inliers;
		const bool equalInliers     = bestForWindow.inliers == bestForN.inliers;
		const bool betterSpan       = bestForWindow.span > bestForN.span;
		const bool equalSpan        = bestForWindow.span == bestForN.span;
		const bool betterGapRms     = gapRms + RMS_EPS < bestGapRmsForN;
		const bool equalGapRms      = std::abs(gapRms - bestGapRmsForN) <= RMS_EPS;
		const bool betterRms        = bestForWindow.rms + RMS_EPS < bestForN.rms;
		const bool equalRms         = std::abs(bestForWindow.rms - bestForN.rms) <= RMS_EPS;
		const bool betterSpacingDev = spacingDevCur + RMS_EPS < spacingDevBest;
		const bool equalSpacingDev  = std::abs(spacingDevCur - spacingDevBest) <= RMS_EPS;
		const bool preferLeftWindow = wStart < bestWindowStartForN;

		const bool selectCurrent =
		        betterObjective ||
		        (equalObjective &&
		         (betterInliers ||
		          (equalInliers &&
		           (betterSpan || (equalSpan && (betterGapRms || (equalGapRms && (betterRms || (equalRms && (betterSpacingDev ||
		                                                                                                     (equalSpacingDev && preferLeftWindow)))))))))));

		if (selectCurrent) {
			bestForN            = bestForWindow;
			bestStartForN       = bestStartForWindow;
			bestSpacingForN     = spacing;
			bestPhaseForN       = phase;
			bestWindowStartForN = wStart;
			bestGapRmsForN      = gapRms;
		}
	}

	if (!std::isfinite(bestForN.rms) || bestForN.inliers == 0u) {
#ifndef NDEBUG
		std::cout << std::format(" - Fit N={}: no valid lattice fit\n", N);
#endif
		return false;
	}

#ifndef NDEBUG
	std::cout << std::format(" - Fit N={}: rms={:.3f}px inliers={}/{} span={} gapRms={:.3f}px windowStart={}\n", N, bestForN.rms, bestForN.inliers, N,
	                         bestForN.span, bestGapRmsForN, bestWindowStartForN);
#endif

	// 4. Reconstruct the full lattice for this N.
	outGrid.resize(N);
	for (std::size_t k = 0u; k < N; ++k) { // k = lattice line index
		outGrid[k] = bestStartForN + static_cast<double>(k) * bestSpacingForN;
	}

#ifndef NDEBUG
	std::cout << std::format("Selected N={} spacing={:.3f} phase={:.3f} offset={} rms={:.3f}px inliers={}/{} windowStart={}\n", N, bestSpacingForN,
	                         bestPhaseForN, bestForN.offset, bestForN.rms, bestForN.inliers, N, bestWindowStartForN);
#endif

	return true;
}

bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters, std::vector<double>& vGrid, std::vector<double>& hGrid,
              const std::vector<std::size_t>& candidateNs) {
	auto isValidN = [](std::size_t n) { // helper: is n a legal board size?
		return n == 9u || n == 13u || n == 19u;
	};

#ifndef NDEBUG
	// Estimate a spacing from the vertical axis for logging / sanity checks.
	std::vector<double> gaps; // Gap size between candidate vertical lines (px)
	gaps.reserve(vCenters.size() - 1);
	for (size_t i = 0; i + 1 < vCenters.size(); ++i) { // i = index into vCenters
		gaps.push_back(vCenters[i + 1] - vCenters[i]);
	}

	double s = modeGap(gaps, 4.0); // rough spacing estimate (px)
	std::cout << "DEBUG: Estimated spacing s=" << s << "\n";
#endif

	std::vector<std::size_t> NsAll; //!< Candidate board sizes to evaluate.
	NsAll.reserve(candidateNs.size());
	for (const auto n: candidateNs) {
		if (isValidN(n)) {
			NsAll.push_back(n);
		}
	}
	if (NsAll.empty()) {
		return false;
	}

	// Jointly select N using both axes. This avoids locking onto a wrong N when one axis happens to
	// have an exact valid count due to missing detections (e.g., 13x13 board with 9 detected lines).
	struct JointCandidate {
		std::size_t N{0u};                                   //!< Board size
		std::size_t inliersTotal{0u};                        //!< ScoreV.inliers + scoreH.inliers
		double completeness{0.0};                            //!< InliersTotal / (2*N)
		double coverage{0.0};                                //!< InliersTotal / (vCenters+hCenters)
		double balanced{0.0};                                //!< Harmonic mean of completeness+coverage
		double rms{std::numeric_limits<double>::infinity()}; //!< Combined weighted RMS (px)
		std::vector<double> v;                               //!< Fitted vertical grid for this N
		std::vector<double> h;                               //!< Fitted horizontal grid for this N
	};

	auto safeRatio = [](const std::size_t num, const std::size_t den) -> double {
		return den == 0u ? 0.0 : static_cast<double>(num) / static_cast<double>(den);
	};

	auto harmonicMean = [](const double a, const double b) -> double {
		const double sum = a + b;
		return (sum <= 0.0) ? 0.0 : (2.0 * a * b) / sum;
	};

	auto isBetterJointCandidate = [](const JointCandidate& lhs, const JointCandidate& rhs) -> bool {
		static constexpr double SCORE_EPS = 1e-12; //!< Epsilon for score comparisons.
		static constexpr double RMS_EPS   = 1e-6;  //!< Epsilon for RMS comparisons.

		const bool betterBalanced = lhs.balanced > rhs.balanced + SCORE_EPS;                    //!< Prefer jointly balanced fit quality.
		const bool equalBalanced  = std::abs(lhs.balanced - rhs.balanced) <= SCORE_EPS;         //!< Tie on balanced score.
		const bool betterComp     = lhs.completeness > rhs.completeness + SCORE_EPS;            //!< Prefer covering required lattice lines.
		const bool equalComp      = std::abs(lhs.completeness - rhs.completeness) <= SCORE_EPS; //!< Tie on completeness.
		const bool betterCover    = lhs.coverage > rhs.coverage + SCORE_EPS;                    //!< Prefer explaining observed detections.
		const bool equalCover     = std::abs(lhs.coverage - rhs.coverage) <= SCORE_EPS;         //!< Tie on coverage.
		const bool betterRms      = lhs.rms + RMS_EPS < rhs.rms;                                //!< Prefer smaller alignment error.
		const bool equalRms       = std::abs(lhs.rms - rhs.rms) <= RMS_EPS;                     //!< Tie on RMS.
		const bool betterInliers  = lhs.inliersTotal > rhs.inliersTotal;                        //!< Prefer more absolute inliers.
		const bool equalInliers   = lhs.inliersTotal == rhs.inliersTotal;                       //!< Tie on inliers.
		const bool preferSmallerN = (rhs.N == 0u) ? true : (lhs.N < rhs.N);                     //!< Deterministic tie-break.

		return betterBalanced ||
		       (equalBalanced &&
		        (betterComp ||
		         (equalComp && (betterCover || (equalCover && (betterRms || (equalRms && (betterInliers || (equalInliers && preferSmallerN)))))))));
	};

	JointCandidate best{}; // best joint candidate so far
	bool hasBest = false;  // whether best is initialized

	// 1 Evaluate each N and keep the best joint score.
	for (auto N: NsAll) {
		std::vector<double> vTmp; //!< Fitted vertical grid for this N
		std::vector<double> hTmp; //!< Fitted horizontal grid for this N

		if (!selectGridByLatticeFit(vCenters, N, vTmp))
			continue;
		if (!selectGridByLatticeFit(hCenters, N, hTmp))
			continue;

		// Just need two lines for spacing calculation
		if (vTmp.size() < 2u || hTmp.size() < 2u)
			continue;

		const double vStart   = vTmp.front();      //!< Vertical grid[0] (px)
		const double vSpacing = vTmp[1] - vTmp[0]; //!< Vertical spacing (px)
		const double hStart   = hTmp.front();      //!< Horizontal grid[0] (px)
		const double hSpacing = hTmp[1] - hTmp[0]; //!< Horizontal spacing (px)
		if (!(vSpacing > 1e-6 && hSpacing > 1e-6 && std::isfinite(vSpacing) && std::isfinite(hSpacing)))
			continue;

		// Re-score against the full candidate lists to get robust inlier counts.
		LatticeScore scoreV{}; //!< Vertical axis score vs all candidates
		LatticeScore scoreH{}; //!< Horizontal axis score vs all candidates
		if (!evaluateLatticeOffset(vCenters, vStart, vSpacing, N, scoreV))
			continue;
		if (!evaluateLatticeOffset(hCenters, hStart, hSpacing, N, scoreH))
			continue;

		const auto totalInliers   = scoreV.inliers + scoreH.inliers;                            //!< Total explained lines
		const double completeness = safeRatio(totalInliers, 2u * N);                            //!< Fraction of lattice lines explained.
		const double coverage     = safeRatio(totalInliers, vCenters.size() + hCenters.size()); //!< Fraction of detections explained.
		const double balanced     = harmonicMean(completeness, coverage);                       //!< Joint quality (avoid N-size bias).
		const double rms          = (totalInliers > 0)                                          //!< Combined weighted RMS (px)
		                                    ? std::sqrt((scoreV.rms * scoreV.rms * static_cast<double>(scoreV.inliers) +
                                                scoreH.rms * scoreH.rms * static_cast<double>(scoreH.inliers)) /
		                                                static_cast<double>(totalInliers))
		                                    : std::numeric_limits<double>::infinity();
		JointCandidate current{};
		current.N            = N;
		current.inliersTotal = totalInliers;
		current.completeness = completeness;
		current.coverage     = coverage;
		current.balanced     = balanced;
		current.rms          = rms;
		current.v            = std::move(vTmp);
		current.h            = std::move(hTmp);

		if (!hasBest || isBetterJointCandidate(current, best)) {
			hasBest = true;
			best    = std::move(current);
		}
	}

	// 3. Must have a valid selection.
	if (!hasBest || best.N == 0)
		return false;

	// 4. Output fitted lattices for both axes.
	//    Even when detections happen to have exactly N candidates, one or more can still be a border artifact
	//    replacing a true grid line. Always using the fitted lattice avoids locking onto that failure mode.
	vGrid = std::move(best.v);
	hGrid = std::move(best.h);

	// 6. Ensure consistency.
	return isValidN(vGrid.size()) && isValidN(hGrid.size()) && vGrid.size() == hGrid.size();
}

bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters, std::vector<double>& vGrid, std::vector<double>& hGrid) {
	static const std::vector<std::size_t> kDefaultNs = {19u, 13u, 9u};
	return findGrid(vCenters, hCenters, vGrid, hGrid, kDefaultNs);
}

} // namespace go::camera
