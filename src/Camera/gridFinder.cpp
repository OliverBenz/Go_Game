#include "gridFinder.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <format>
#include <limits>
#include <vector>
#include <iostream>

namespace go::camera {

namespace debugging {
// TODO: Add to debug code with DebugVisualizer. (Extend visualizer to allow subStages)
/* Usage: 
 *   if (debugger) debugger->add("Gap Histogram", debugging:drawGapHistogram(gaps, 4.0));
 */

// cv::Mat drawGapHistogram(const std::vector<double>& gaps, double binWidth)
// {
//     if (gaps.empty()) return {};

//     double gmin = *std::min_element(gaps.begin(), gaps.end());
//     double gmax = *std::max_element(gaps.begin(), gaps.end());

//     int bins = static_cast<int>(std::ceil((gmax - gmin) / binWidth)) + 1;
//     std::vector<int> hist(bins, 0);

//     for (double g : gaps) {
//         int b = static_cast<int>(std::floor((g - gmin) / binWidth));
//         if (b >= 0 && b < bins) hist[b]++;
//     }

//     int maxCount = *std::max_element(hist.begin(), hist.end());

//     const int width = 800;
//     const int height = 400;
//     const int margin = 40;

//     cv::Mat histImg(height, width, CV_8UC3, cv::Scalar(20,20,20));

//     double binPixelWidth = static_cast<double>(width - 2*margin) / bins;

//     for (int i = 0; i < bins; ++i) {
//         double ratio = static_cast<double>(hist[i]) / maxCount;
//         int barHeight = static_cast<int>(ratio * (height - 2*margin));

//         int x1 = margin + static_cast<int>(i * binPixelWidth);
//         int x2 = margin + static_cast<int>((i+1) * binPixelWidth);
//         int y1 = height - margin;
//         int y2 = height - margin - barHeight;

//         cv::rectangle(histImg,
//                       cv::Point(x1, y1),
//                       cv::Point(x2, y2),
//                       cv::Scalar(0, 200, 255),
//                       cv::FILLED);
//     }

//     cv::putText(histImg, "Gap Histogram",
//                 cv::Point(20, 30),
//                 cv::FONT_HERSHEY_SIMPLEX,
//                 0.8,
//                 cv::Scalar(255,255,255),
//                 2);

//     return histImg;
// }
}


//! Construct histogramm of gap sizes to get best fitting size.
double modeGap(const std::vector<double>& gaps, double binWidth)
{
    if (gaps.empty()) return 0.0;

    double gmin = *std::min_element(gaps.begin(), gaps.end());
    double gmax = *std::max_element(gaps.begin(), gaps.end());

    int bins = (int)std::ceil((gmax - gmin) / binWidth) + 1;
    std::vector<int> hist(bins, 0);

    for (double g : gaps) {
        int b = (int)std::floor((g - gmin) / binWidth);
        if (b >= 0 && b < bins) hist[b]++;
    }

    int bestBin = (int)(std::max_element(hist.begin(), hist.end()) - hist.begin());
    
	// return center of best bin
    return gmin + (bestBin + 0.5) * binWidth;
}

static double positiveFmod(double x, double period) {
    if (period <= 0.0) return 0.0;
    double r = std::fmod(x, period);
    if (r < 0.0) r += period;
    if (r >= period) r = 0.0;
    return r;
}

static double dominantResidualPhase(const std::vector<double>& centersSorted, double spacing) {
    if (centersSorted.empty() || spacing <= 0.0) return 0.0;

    // Residuals r_i = c_i mod spacing, wrapped to [0, spacing).
    std::vector<double> residuals;
    residuals.reserve(centersSorted.size());
    for (double c : centersSorted) {
        residuals.push_back(positiveFmod(c, spacing));
    }

    // Histogram residuals to find dominant phase cluster.
    const double binWidth = std::clamp(0.04 * spacing, 1.0, 4.0); // ~3px for typical spacing ~75px
    const int bins = std::max(8, static_cast<int>(std::ceil(spacing / binWidth)));
    std::vector<int> hist(static_cast<std::size_t>(bins), 0);

    for (double r : residuals) {
        int b = static_cast<int>(std::floor(r / binWidth));
        b = std::clamp(b, 0, bins - 1);
        hist[static_cast<std::size_t>(b)]++;
    }

    auto smoothedCount = [&](int i) -> int {
        const int prev = (i - 1 + bins) % bins;
        const int next = (i + 1) % bins;
        return hist[static_cast<std::size_t>(prev)] + hist[static_cast<std::size_t>(i)] + hist[static_cast<std::size_t>(next)];
    };

    int bestBin = 0;
    int bestCount = -1;
    for (int i = 0; i < bins; ++i) {
        const int c = smoothedCount(i);
        if (c > bestCount) {
            bestCount = c;
            bestBin = i;
        }
    }

    const int prev = (bestBin - 1 + bins) % bins;
    const int next = (bestBin + 1) % bins;

    // Compute circular mean of residuals inside the dominant bin window (bestBin +/- 1) to get sub-bin phase.
    static constexpr double TWO_PI = 6.28318530717958647692;
    double sumSin = 0.0;
    double sumCos = 0.0;
    int used = 0;

    for (double r : residuals) {
        int b = static_cast<int>(std::floor(r / binWidth));
        b = std::clamp(b, 0, bins - 1);
        if (b != bestBin && b != prev && b != next) continue;

        const double ang = TWO_PI * (r / spacing);
        sumSin += std::sin(ang);
        sumCos += std::cos(ang);
        ++used;
    }

    if (used == 0 || (std::abs(sumSin) + std::abs(sumCos)) < 1e-12) {
        // Fallback: center of dominant bin.
        const double phase = (static_cast<double>(bestBin) + 0.5) * binWidth;
        return std::clamp(phase, 0.0, std::nextafter(spacing, 0.0));
    }

    double ang = std::atan2(sumSin, sumCos);
    if (ang < 0.0) ang += TWO_PI;
    const double phase = (ang / TWO_PI) * spacing;
    return std::clamp(phase, 0.0, std::nextafter(spacing, 0.0));
}

struct LatticeScore {
    double rms{std::numeric_limits<double>::infinity()};
    int inliers{0};
    int span{0};
    int offset{0}; // integer multiple of spacing applied to phase
};

static bool evaluateLatticeOffset(const std::vector<double>& centersSorted,
                                  double start,
                                  double spacing,
                                  int N,
                                  LatticeScore& outScore)
{
    if (N <= 0 || spacing <= 0.0) return false;

    // For each grid index k, keep the closest detected center (best absolute residual).
    std::vector<double> bestErr(static_cast<std::size_t>(N), std::numeric_limits<double>::infinity());
    std::vector<uint8_t> has(static_cast<std::size_t>(N), 0u);

    for (double c : centersSorted) {
        const double kReal = (c - start) / spacing;
        const int k = static_cast<int>(std::lround(kReal));
        if (k < 0 || k >= N) continue;

        const double predicted = start + static_cast<double>(k) * spacing;
        const double e = c - predicted;
        const double ae = std::abs(e);

        const std::size_t ki = static_cast<std::size_t>(k);
        if (has[ki] == 0u || ae < std::abs(bestErr[ki])) {
            bestErr[ki] = e;
            has[ki] = 1u;
        }
    }

    double sumSq = 0.0;
    int inliers = 0;
    int minK = N;
    int maxK = -1;
    for (int k = 0; k < N; ++k) {
        const std::size_t ki = static_cast<std::size_t>(k);
        if (has[ki] == 0u) continue;
        ++inliers;
        sumSq += bestErr[ki] * bestErr[ki];
        minK = std::min(minK, k);
        maxK = std::max(maxK, k);
    }

    if (inliers == 0) return false;

    outScore.rms = std::sqrt(sumSq / static_cast<double>(inliers));
    outScore.inliers = inliers;
    outScore.span = (maxK >= minK) ? (maxK - minK + 1) : 0;
    return true;
}

static bool selectGridByLatticeFit(const std::vector<double>& centersSorted,
                                   const std::vector<int>& Ns,
                                   std::vector<double>& outGrid,
                                   int& outN)
{
    outGrid.clear();
    outN = 0;

    if (centersSorted.size() < 6) return false;

    LatticeScore bestOverall{};
    int bestN = 0;
    double bestStart = 0.0;
    double bestSpacing = 0.0;
    double bestPhase = 0.0;
    std::size_t bestWindowStart = 0;
    double bestGapRms = std::numeric_limits<double>::infinity();

    for (int N : Ns) {
        if (N <= 0) continue;

        LatticeScore bestForN{};
        double bestStartForN = 0.0;
        double bestSpacingForN = 0.0;
        double bestPhaseForN = 0.0;
        std::size_t bestWindowStartForN = 0;
        double bestGapRmsForN = std::numeric_limits<double>::infinity();

        // If we have more candidates than N, prefer a contiguous window of size N.
        // This naturally rejects spurious physical board borders at the extremes.
        const std::size_t M = centersSorted.size();
        const std::size_t windowSize = (M >= static_cast<std::size_t>(N)) ? static_cast<std::size_t>(N) : M;
        const std::size_t windows = (M >= static_cast<std::size_t>(N)) ? (M - static_cast<std::size_t>(N) + 1) : 1;

        for (std::size_t w = 0; w < windows; ++w) {
            const std::size_t wStart = (windows == 1) ? 0 : w;

            const auto beginIt = centersSorted.begin() + static_cast<std::ptrdiff_t>(wStart);
            const auto endIt = beginIt + static_cast<std::ptrdiff_t>(windowSize);
            const std::vector<double> centersWindow(beginIt, endIt);

            if (centersWindow.size() < 2) continue;

            // Spacing estimate from adjacent gaps (on this window to reduce influence of border outliers).
            std::vector<double> gaps;
            gaps.reserve(centersWindow.size() - 1);
            for (std::size_t i = 0; i + 1 < centersWindow.size(); ++i) {
                gaps.push_back(centersWindow[i + 1] - centersWindow[i]);
            }

            const double spacing = modeGap(gaps, 4.0);
            if (!(spacing > 1e-6 && std::isfinite(spacing))) continue;

            // Gap regularity: true grid lines have near-constant adjacent gaps; border artifacts create outlier gaps.
            double gapSumSq = 0.0;
            for (double g : gaps) {
                const double d = g - spacing;
                gapSumSq += d * d;
            }
            const double gapRms = gaps.empty()
                ? std::numeric_limits<double>::infinity()
                : std::sqrt(gapSumSq / static_cast<double>(gaps.size()));

            const double phase = dominantResidualPhase(centersWindow, spacing);

            // Candidate integer offsets: start = phase + offset * spacing
            // Enumerate offsets derived from mapping detected centers to lattice indices.
            std::vector<int> kApprox;
            kApprox.reserve(centersWindow.size());
            for (double c : centersWindow) {
                kApprox.push_back(static_cast<int>(std::lround((c - phase) / spacing)));
            }

            std::vector<int> offsets;
            offsets.reserve(kApprox.size() * static_cast<std::size_t>(N));
            for (int k : kApprox) {
                for (int j = 0; j < N; ++j) {
                    offsets.push_back(k - j);
                }
            }

            std::sort(offsets.begin(), offsets.end());
            offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());

            LatticeScore bestForWindow{};
            double bestStartForWindow = 0.0;

            for (int offset : offsets) {
                const double start = phase + static_cast<double>(offset) * spacing;

                LatticeScore score{};
                score.offset = offset;
                if (!evaluateLatticeOffset(centersWindow, start, spacing, N, score)) continue;

                // Selection criteria for this window:
                // maximize explained structure first (inliers/span), then minimize alignment error.
                static constexpr double RMS_EPS = 1e-6;
                const bool betterInliers = score.inliers > bestForWindow.inliers;
                const bool equalInliers = score.inliers == bestForWindow.inliers;
                const bool betterSpan = score.span > bestForWindow.span;
                const bool equalSpan = score.span == bestForWindow.span;
                const bool betterRms = score.rms + RMS_EPS < bestForWindow.rms;
                const bool equalRms = std::abs(score.rms - bestForWindow.rms) <= RMS_EPS;
                const bool betterOffset = std::abs(score.offset) < std::abs(bestForWindow.offset);

                if (betterInliers
                    || (equalInliers && (betterSpan
                        || (equalSpan && (betterRms
                            || (equalRms && betterOffset))))))
                {
                    bestForWindow = score;
                    bestStartForWindow = start;
                }
            }

            if (!std::isfinite(bestForWindow.rms) || bestForWindow.inliers == 0) {
                continue;
            }

            // Select best window for this N.
            static constexpr double RMS_EPS = 1e-6;
            const bool betterInliers = bestForWindow.inliers > bestForN.inliers;
            const bool equalInliers = bestForWindow.inliers == bestForN.inliers;
            const bool betterSpan = bestForWindow.span > bestForN.span;
            const bool equalSpan = bestForWindow.span == bestForN.span;
            const bool betterGapRms = gapRms + RMS_EPS < bestGapRmsForN;
            const bool equalGapRms = std::abs(gapRms - bestGapRmsForN) <= RMS_EPS;
            const bool betterRms = bestForWindow.rms + RMS_EPS < bestForN.rms;
            const bool equalRms = std::abs(bestForWindow.rms - bestForN.rms) <= RMS_EPS;
            const bool preferLeftWindow = wStart < bestWindowStartForN;

            if (betterInliers
                || (equalInliers && (betterSpan
                    || (equalSpan && (betterGapRms
                        || (equalGapRms && (betterRms
                            || (equalRms && preferLeftWindow))))))))
            {
                bestForN = bestForWindow;
                bestStartForN = bestStartForWindow;
                bestSpacingForN = spacing;
                bestPhaseForN = phase;
                bestWindowStartForN = wStart;
                bestGapRmsForN = gapRms;
            }
        }

        if (!std::isfinite(bestForN.rms) || bestForN.inliers == 0) {
#ifndef NDEBUG
            std::cout << std::format(" - Fit N={}: no valid lattice fit\n", N);
#endif
            continue;
        }

#ifndef NDEBUG
        std::cout << std::format(" - Fit N={}: rms={:.3f}px inliers={}/{} span={} gapRms={:.3f}px windowStart={}\n",
                                 N, bestForN.rms, bestForN.inliers, N, bestForN.span, bestGapRmsForN, bestWindowStartForN);
#endif

        // Choose global best:
        // prefer the board size that explains the most detected lines (absolute inliers),
        // then break ties by completeness (inlier ratio), then by RMS (alignment).
        const double ratio = static_cast<double>(bestForN.inliers) / static_cast<double>(N);
        const double bestRatio = (bestN == 0) ? -1.0 : (static_cast<double>(bestOverall.inliers) / static_cast<double>(bestN));

        static constexpr double RATIO_EPS = 1e-12;
        static constexpr double RMS_EPS = 1e-6;

        const bool betterInliers = bestForN.inliers > bestOverall.inliers;
        const bool equalInliers = bestForN.inliers == bestOverall.inliers;
        const bool betterRatio = ratio > bestRatio + RATIO_EPS;
        const bool equalRatio = std::abs(ratio - bestRatio) <= RATIO_EPS;
        const bool betterRms = bestForN.rms + RMS_EPS < bestOverall.rms;
        const bool equalRms = std::abs(bestForN.rms - bestOverall.rms) <= RMS_EPS;
        const bool preferSmallerN = (bestN == 0) ? true : (N < bestN);

        if (betterInliers
            || (equalInliers && (betterRatio
                || (equalRatio && (betterRms
                    || (equalRms && preferSmallerN)))))) {
            bestOverall = bestForN;
            bestN = N;
            bestStart = bestStartForN;
            bestSpacing = bestSpacingForN;
            bestPhase = bestPhaseForN;
            bestWindowStart = bestWindowStartForN;
            bestGapRms = bestGapRmsForN;
        }
    }

    if (bestN == 0) return false;

    outN = bestN;
    outGrid.resize(static_cast<std::size_t>(outN));
    for (int k = 0; k < outN; ++k) {
        outGrid[static_cast<std::size_t>(k)] = bestStart + static_cast<double>(k) * bestSpacing;
    }

#ifndef NDEBUG
    std::cout << std::format("Selected N={} spacing={:.3f} phase={:.3f} offset={} rms={:.3f}px inliers={}/{} windowStart={}\n",
                             outN, bestSpacing, bestPhase, bestOverall.offset, bestOverall.rms, bestOverall.inliers, outN, bestWindowStart);
#endif

    return true;
}


bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters,
    std::vector<double>& vGrid, std::vector<double>& hGrid)
{
    auto isValidN = [](std::size_t n) {
        return n == 9u || n == 13u || n == 19u;
    };

    // Calculate gaps between the selected lines
	std::vector<double> gaps; //!< Gap sized between the grid lines.
	gaps.reserve(vCenters.size() - 1);
	for (size_t i = 0; i + 1 < vCenters.size(); ++i) {
		gaps.push_back(vCenters[i + 1] - vCenters[i]);
	}

#ifndef NDEBUG
	// // // Construct histogramm of gaps and select highest bin
	double s = modeGap(gaps, 4.0);
	std::cout << "DEBUG: Estimated spacing s=" << s << "\n";
#endif

    // Possible board sizes
    const std::vector<int> NsAll = {19, 13, 9};

    // Jointly select N using both axes. This avoids locking onto a wrong N when one axis
    // happens to have an exact valid count due to missing detections (e.g., 13x13 board with 9 detected lines).
    struct JointCandidate {
        int N{0};
        int inliersTotal{0};
        double ratio{0.0};
        double rms{std::numeric_limits<double>::infinity()};
        std::vector<double> v;
        std::vector<double> h;
    };

    JointCandidate best{};
    bool hasBest = false;

    for (int N : NsAll) {
        std::vector<double> vTmp;
        std::vector<double> hTmp;
        int Nv = 0;
        int Nh = 0;

        if (!selectGridByLatticeFit(vCenters, std::vector<int>{N}, vTmp, Nv) || Nv != N) continue;
        if (!selectGridByLatticeFit(hCenters, std::vector<int>{N}, hTmp, Nh) || Nh != N) continue;
        if (vTmp.size() < 2u || hTmp.size() < 2u) continue;

        const double vStart = vTmp.front();
        const double vSpacing = vTmp[1] - vTmp[0];
        const double hStart = hTmp.front();
        const double hSpacing = hTmp[1] - hTmp[0];
        if (!(vSpacing > 1e-6 && hSpacing > 1e-6 && std::isfinite(vSpacing) && std::isfinite(hSpacing))) continue;

        LatticeScore scoreV{};
        LatticeScore scoreH{};
        if (!evaluateLatticeOffset(vCenters, vStart, vSpacing, N, scoreV)) continue;
        if (!evaluateLatticeOffset(hCenters, hStart, hSpacing, N, scoreH)) continue;

        const int totalInliers = scoreV.inliers + scoreH.inliers;
        const double ratio = static_cast<double>(totalInliers) / static_cast<double>(2 * N);
        const int used = totalInliers;
        const double rms = (used > 0)
            ? std::sqrt((scoreV.rms * scoreV.rms * static_cast<double>(scoreV.inliers)
                       + scoreH.rms * scoreH.rms * static_cast<double>(scoreH.inliers))
                       / static_cast<double>(used))
            : std::numeric_limits<double>::infinity();

        static constexpr double RATIO_EPS = 1e-12;
        static constexpr double RMS_EPS = 1e-6;

        const bool betterInliers = totalInliers > best.inliersTotal;
        const bool equalInliers = totalInliers == best.inliersTotal;
        const bool betterRatio = ratio > best.ratio + RATIO_EPS;
        const bool equalRatio = std::abs(ratio - best.ratio) <= RATIO_EPS;
        const bool betterRms = rms + RMS_EPS < best.rms;
        const bool equalRms = std::abs(rms - best.rms) <= RMS_EPS;
        const bool preferSmallerN = (best.N == 0) ? true : (N < best.N);

        if (!hasBest
            || betterInliers
            || (equalInliers && (betterRatio
                || (equalRatio && (betterRms
                    || (equalRms && preferSmallerN)))))) {
            hasBest = true;
            best.N = N;
            best.inliersTotal = totalInliers;
            best.ratio = ratio;
            best.rms = rms;
            best.v.swap(vTmp);
            best.h.swap(hTmp);
        }
    }

    if (!hasBest || best.N == 0) return false;

    // If an axis already has exactly N candidates, keep them as-is to avoid introducing
    // small phase shifts from refitting (stone detection is sensitive to intersection jitter).
    // Otherwise use the fitted lattice.
    vGrid = (vCenters.size() == static_cast<std::size_t>(best.N)) ? vCenters : std::move(best.v);
    hGrid = (hCenters.size() == static_cast<std::size_t>(best.N)) ? hCenters : std::move(best.h);
    return isValidN(vGrid.size()) && isValidN(hGrid.size()) && vGrid.size() == hGrid.size();
}

}
