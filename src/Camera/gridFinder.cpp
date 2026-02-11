#include "gridFinder.hpp"

#include <cmath>
#include <format>
#include <vector>
#include <iostream>

namespace go::camera {

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

bool nearestWithin(const std::vector<double>& centersSorted,
                          double target, double tol,
                          double& outValue)
{
    auto it = std::lower_bound(centersSorted.begin(), centersSorted.end(), target);

    double best = 0.0;
    double bestErr = 1e18;
    bool found = false;

    if (it != centersSorted.end()) {
        double err = std::abs(*it - target);
        if (err < bestErr) { bestErr = err; best = *it; found = true; }
    }
    if (it != centersSorted.begin()) {
        double v = *(it - 1);
        double err = std::abs(v - target);
        if (err < bestErr) { bestErr = err; best = v; found = true; }
    }

    if (found && bestErr <= tol) {
        outValue = best;
        return true;
    }
    return false;
}

std::vector<double> buildGridFromStart(const std::vector<double>& centersSorted,
                                              double startPos, double spacing,
                                              int N, double tol)
{
    std::vector<double> grid;
    grid.reserve(N);

    for (int k = 0; k < N; ++k) {
        double target = startPos + k * spacing;
        double snapped = 0.0;
        if (!nearestWithin(centersSorted, target, tol, snapped)) {
            return {}; // fail
        }
        // avoid duplicates due to snapping
        if (!grid.empty() && std::abs(snapped - grid.back()) < 1e-6) return {};
        grid.push_back(snapped);
    }
    return grid;
}

//! Compute coverage count. Measure for the ability to predict neighbouring lines.
int coverageCount(const std::vector<double>& centersSorted,
                         double start,
                         double spacing,
                         int N,
                         double tol)
{
    int count = 0;

    for (double c : centersSorted)
    {
        // Compute nearest grid index k
        double kReal = (c - start) / spacing;
        int k = (int)std::lround(kReal);

        // Check if that index is inside grid
        if (k < 0 || k >= N)
            continue;

        double predicted = start + k * spacing;

        if (std::abs(c - predicted) <= tol)
            count++;
    }

    return count;
}

bool selectGridByProgression(const std::vector<double>& centersSorted,
                                    const std::vector<int>& Ns,
                                    std::vector<double>& outGrid,
                                    int& outN)
{
    outGrid.clear();
    outN = 0;

    if (centersSorted.size() < 9) return false;

    // spacing estimate from adjacent gaps
    std::vector<double> gaps;
    gaps.reserve(centersSorted.size() - 1);
    for (size_t i = 0; i + 1 < centersSorted.size(); ++i)
        gaps.push_back(centersSorted[i + 1] - centersSorted[i]);

    double s = modeGap(gaps, 4.0);
    if (s <= 1e-6) return false;

    // tolerance: allow some drift / warp error. 0.25*s is usually safe.
    double tol = 0.25 * s;

    double bestScore = 1e18;

    for (int N : Ns) {
        double bestN = 1e18; // For debuggin purpose

        for (double start : centersSorted) {
            auto grid = buildGridFromStart(centersSorted, start, s, N, tol);
            if (grid.empty()) continue;

            // Measure if this hypothetical grid can detect neighbouring lines in the given gap size.
            // Used to favor larger grid configurations.
            int coverage = coverageCount(centersSorted, start, s, N, tol);

            // score: sum of |grid[k] - (start + k*s)|
            double score = 0.0;
            for (int k = 0; k < N; ++k) {
                double target = start + k * s;
                score += std::abs(grid[k] - target);
            }

            // normalize: average error per line, relative to spacing
            static constexpr double lambda = 0.02; //!< Weight to include coverage in weighted mean.
            score = (score / (double)N) / s - lambda * coverage;

            if (score < bestN) {
                bestN = score;
            }

            const double preferLargerMargin = 0.15; // 15%
            if (score < bestScore  || (outN != 0 && score <= bestScore * (1.0 + preferLargerMargin) && N > outN)) {
                bestScore = score;
                outGrid = std::move(grid);
                outN = N;
            }
        }
        
        std::cout << std::format(" - Best score for size {}: {}\n", N, bestN);
    }

    std::cout << "Selected N=" << outN << " spacing=" << s
              << " tol=" << tol << " score=" << bestScore << "\n";

    return outN != 0;
}


bool findGrid(const std::vector<double>& vCenters, const std::vector<double>& hCenters,
    std::vector<double>& vGrid, std::vector<double>& hGrid)
{
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

    // Progression scanning (calculate mean grid distances -> construct ideal grid -> snap closest lines -> score accuracy)
    std::vector<int> Ns = {19, 13, 9}; //!< Possible board sizes

    vGrid.clear();
	int Nv = 0;
	bool okV = selectGridByProgression(vCenters, Ns, vGrid, Nv);

    hGrid.clear();
	int Nh = 0;
	bool okH = selectGridByProgression(hCenters, Ns, hGrid, Nh);

    return okV && okH && Nv == Nh;
}

}
