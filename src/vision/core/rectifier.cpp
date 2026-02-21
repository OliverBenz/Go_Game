#include "camera/rectifier.hpp"

#include "camera/boardFinder.hpp"

#include "gridFinder.hpp"
#include "statistics.hpp"

#include <algorithm>
#include <cmath>

namespace tengen::vision::core {
namespace debugging {

//! Draw lines onto an image.
cv::Mat drawLines(const cv::Mat& image, const std::vector<double>& vertical, const std::vector<double>& horizontal) {
	cv::Mat drawnLines = image.clone(); // Use warped image ideally

	for (double x: vertical) {
		int xi = static_cast<int>(std::lround(x));
		cv::line(drawnLines, cv::Point(xi, 0), cv::Point(xi, drawnLines.rows - 1), cv::Scalar(255, 0, 0), 3);
	}

	// Draw clustered horizontal grid lines (thick yellow)
	for (double y: horizontal) {
		int yi = static_cast<int>(std::lround(y));
		cv::line(drawnLines, cv::Point(0, yi), cv::Point(drawnLines.cols - 1, yi), cv::Scalar(100, 0, 150), 3);
	}
	return drawnLines;
}

} // namespace debugging

struct Line1D {
	double pos;    // x for vertical, y for horizontal
	double weight; // e.g. segment length
};

static bool isValidBoardSize(std::size_t n) {
	return n == 9u || n == 13u || n == 19u;
}

static std::vector<double> clusterWeighted1D(std::vector<Line1D> values, double eps) {
	if (values.empty()) {
		return {};
	};

	// Sort lines by position
	std::sort(values.begin(), values.end(), [](const Line1D& a, const Line1D& b) { return a.pos < b.pos; });

	std::vector<double> centers;
	double wSum = values[0].weight;
	double pSum = values[0].pos * values[0].weight;
	for (size_t i = 1; i < values.size(); ++i) {
		if (std::abs(values[i].pos - values[i - 1].pos) <= eps) {
			wSum += values[i].weight;
			pSum += values[i].pos * values[i].weight;
		} else {
			centers.push_back(pSum / wSum);
			wSum = values[i].weight;
			pSum = values[i].pos * values[i].weight;
		}
	}
	centers.push_back(pSum / wSum);

	return centers;
}

static double computeMedianSpacing(const std::vector<double>& grid) {
	if (grid.size() < 2u) {
		return 0.0;
	}

	std::vector<double> diffs;
	diffs.reserve(grid.size() - 1);
	for (size_t i = 1; i < grid.size(); ++i)
		diffs.push_back(grid[i] - grid[i - 1]);

	return median(diffs);
}

//! Transform an image that contains a Go Board such that the final image is a top-down projection of the board.
//! \note The border of the image is the outermost grid line + tolerance for the edge stones.
BoardGeometry rectifyImage(const cv::Mat& originalImg, const WarpResult& input, DebugVisualizer* debugger) {
	if (input.image.empty() || input.H.empty()) {
		std::cerr << "Invalid warp result for rectification.\n";
		return {};
	}

	if (debugger) {
		debugger->beginStage("Rectify Image");
		debugger->add("Input", input.image);
	}

	// TODO: Properly rotate at some point. Roughly rotate in warpToBoard() and fine rotate here.


	// 1. Preprocess again
	cv::Mat gray, blur, edges;
	cv::cvtColor(input.image, gray, cv::COLOR_BGR2GRAY); // Greyscale
	if (debugger)
		debugger->add("Grayscale", gray);

	cv::GaussianBlur(gray, blur, cv::Size(9, 9), 1.5); // Blur to reduce noise
	if (debugger)
		debugger->add("Gaussian Blur", blur);

	cv::Canny(blur, edges, 50, 120); // Edge detection
	if (debugger)
		debugger->add("Canny Edge", edges);

	cv::dilate(edges, edges, cv::Mat(), cv::Point(-1, -1), 1); // Cleanup detected edges
	if (debugger)
		debugger->add("Dilate Canny", edges);

	// 2. Find horizontal and vertical line candidates (merge close together lines but there are not necessarily our grid yet)
	// Find line segments
	std::vector<cv::Vec4i> lines;
	cv::HoughLinesP(edges, lines,
	                1,           // rho resolution
	                CV_PI / 180, // theta resolution
	                80,          // threshold (votes)
	                100,         // minLineLength
	                20           // maxLineGap
	);

	// Separate horizontal / vertical lines
	std::vector<cv::Vec4i> vertical;
	std::vector<cv::Vec4i> horizontal;

	for (const auto& l: lines) {
		float dx = l[2] - l[0];
		float dy = l[3] - l[1];

		float angle = std::atan2(dy, dx) * 180.0f / CV_PI;

		// Normalize angle to [-90, 90]
		while (angle < -90)
			angle += 180;
		while (angle > 90)
			angle -= 180;

		if (std::abs(angle) < 15) {
			horizontal.push_back(l);
		} else if (std::abs(angle) > 75) {
			vertical.push_back(l);
		}
	}
	std::cout << "Vertical lines: " << vertical.size() << "\n Horizontal lines: " << horizontal.size() << "\n";

	// Group together lines (one grid line has finite thickness -> detected as many lines)
	std::vector<Line1D> v1d, h1d;
	auto segLen = [](const cv::Vec4i& l) {
		double dx = l[2] - l[0];
		double dy = l[3] - l[1];
		return std::sqrt(dx * dx + dy * dy);
	};
	for (const auto& l: vertical) {
		v1d.push_back({0.5 * (l[0] + l[2]), segLen(l)});
	}
	for (const auto& l: horizontal) {
		h1d.push_back({0.5 * (l[1] + l[3]), segLen(l)});
	}

	// Merge lines close together
	double mergeEps = 15.0; //!< In pixels
	auto vGrid      = clusterWeighted1D(v1d, mergeEps);
	auto hGrid      = clusterWeighted1D(h1d, mergeEps);

	// Filter out physical board border artifacts:
	// A true grid line is crossed by many orthogonal line segments, while the physical board border is not.
	{
		const double coverTol      = 8.0;
		auto computeCoverageCounts = [&](const std::vector<double>& centers, const std::vector<cv::Vec4i>& orthSegments, bool centersAreX) -> std::vector<int> {
			std::vector<int> counts(centers.size(), 0);
			for (std::size_t i = 0; i < centers.size(); ++i) {
				const double c = centers[i];
				int cnt        = 0;
				for (const auto& s: orthSegments) {
					const double a  = centersAreX ? static_cast<double>(s[0]) : static_cast<double>(s[1]);
					const double b  = centersAreX ? static_cast<double>(s[2]) : static_cast<double>(s[3]);
					const double mn = std::min(a, b) - coverTol;
					const double mx = std::max(a, b) + coverTol;
					if (c >= mn && c <= mx)
						++cnt;
				}
				counts[i] = cnt;
			}
			return counts;
		};

		auto pruneEdgeArtifactsByCoverage = [&](std::vector<double>& centers, const std::vector<int>& counts) {
			if (centers.size() < 3)
				return;
			const int maxCount = *std::max_element(counts.begin(), counts.end());
			if (maxCount <= 0)
				return;

			// Only prune obvious low-coverage artifacts at the edges.
			const int thresh = std::max(1, static_cast<int>(std::lround(0.15 * static_cast<double>(maxCount))));

			std::vector<double> pruned;
			pruned.reserve(centers.size());
			for (std::size_t i = 0; i < centers.size(); ++i) {
				const bool isEdge = (i == 0u) || (i + 1u == centers.size());
				if (isEdge && counts[i] < thresh)
					continue;
				pruned.push_back(centers[i]);
			}

			// Only accept pruning if we still have a plausible board dimension.
			if (pruned.size() >= 9u)
				centers.swap(pruned);
		};

		const auto vCoverage = computeCoverageCounts(vGrid, horizontal, /*centersAreX=*/true);
		pruneEdgeArtifactsByCoverage(vGrid, vCoverage);

		const auto hCoverage = computeCoverageCounts(hGrid, vertical, /*centersAreX=*/false);
		pruneEdgeArtifactsByCoverage(hGrid, hCoverage);
	}

	const auto Nv = vGrid.size();
	const auto Nh = hGrid.size();

	std::cout << "Unique vertical candidates: " << Nv << "\n";
	std::cout << "Unique horizontal candidates: " << Nh << "\n";
	if (debugger) {
		debugger->add("Grid Candidates", debugging::drawLines(input.image, vGrid, hGrid));
	}

	// 3. Grid candidates to proper grid.
	// Check if grid found. Else try with another algorithm.
	if (Nv == Nh && (Nv == 9 || Nv == 13 || Nv == 19)) {
		std::cout << "Board size determined directly: " << Nv << "\n";

#ifndef NDEBUG
		// Debug: Verify if the grid is found with a second algorithm.
		std::vector<double> vGridTest{}, hGridTest{};
		const std::vector<std::size_t> validationNs = {Nv};
		const bool validated                        = findGrid(vGrid, hGrid, vGridTest, hGridTest, validationNs);
		if (!validated) {
			std::cerr << "DEBUG: Could not validate the detected grid with the second algorithm for N=" << Nv << ".\n";
		} else if (vGridTest.size() != hGridTest.size() || vGridTest.size() != vGrid.size() || hGridTest.size() != hGrid.size()) {
			std::cerr << "DEBUG: Validation grid size mismatch. directN=" << Nv << " validatedN=" << vGridTest.size() << ".\n";
		}
#endif
	} else {
		std::cout << "Could not detect the board size trivially. Performing further steps.\n";

		std::vector<double> vGridAttempt{};
		std::vector<double> hGridAttempt{};
		if (!findGrid(vGrid, hGrid, vGridAttempt, hGridAttempt)) {
			std::cerr << "Could not detect a valid grid. Stopping!\n";
			return {};
		}

		vGrid = vGridAttempt;
		hGrid = hGridAttempt;
	}

	// Starting here, we assume grid found
	if (vGrid.size() != hGrid.size() || !isValidBoardSize(vGrid.size())) {
		std::cerr << "Invalid grid size after fitting.\n";
		return {};
	}

	// 4. Warp image with stone buffer at edge (want to detect full stone at edge)
	double spacingX = computeMedianSpacing(vGrid);
	double spacingY = computeMedianSpacing(hGrid);
	double spacing  = 0.5 * (spacingX + spacingY); //!< Spacing between grid lines.

	double stoneBuffer = 0.5 * spacing; // NOTE: Could adjust 0.5 to account for imperfect placement.

	float xmin = static_cast<float>(vGrid.front() - stoneBuffer);
	float xmax = static_cast<float>(vGrid.back() + stoneBuffer);
	float ymin = static_cast<float>(hGrid.front() - stoneBuffer);
	float ymax = static_cast<float>(hGrid.back() + stoneBuffer);

	// Perform the warping on the original image (avoid getting black bars if the new image is larger than the warped(after first step) one)
	// Board coordinates in the warped image
	std::vector<cv::Point2f> srcWarped = {{xmin, ymin}, {xmax, ymin}, {xmax, ymax}, {xmin, ymax}};

	// Invert the previous warping so we can apply our new warp on the original image.
	cv::Mat Hinv = input.H.inv();
	std::vector<cv::Point2f> srcOriginal;
	cv::perspectiveTransform(srcWarped, srcOriginal, Hinv);

	// Output range
	const int outSize            = 1000;
	std::vector<cv::Point2f> dst = {{0.f, 0.f}, {(float)outSize - 1.f, 0.f}, {(float)outSize - 1.f, (float)outSize - 1.f}, {0.f, (float)outSize - 1.f}};

	cv::Mat homographyFinal = cv::getPerspectiveTransform(srcOriginal, dst);
	cv::Mat refined;
	cv::warpPerspective(originalImg, refined, homographyFinal, cv::Size(outSize, outSize));
	if (debugger) {
		debugger->add("Warp Image", refined);
	}

	// 5. Compute and warp intersections
	std::vector<cv::Point2f> intersectionsWarped;
	intersectionsWarped.reserve(vGrid.size() * hGrid.size());

	for (double x: vGrid) {
		for (double y: hGrid) {
			intersectionsWarped.emplace_back(static_cast<float>(x), static_cast<float>(y));
		}
	}

	// Warp back to Original
	std::vector<cv::Point2f> intersectionsOriginal;
	cv::perspectiveTransform(intersectionsWarped, intersectionsOriginal, Hinv);

	// Map to refined image
	std::vector<cv::Point2f> intersectionsRefined;
	cv::perspectiveTransform(intersectionsOriginal, intersectionsRefined, homographyFinal);
	if (debugger) {
		cv::Mat vis = refined.clone();
		for (const auto& p: intersectionsRefined)
			cv::circle(vis, p, 4, cv::Scalar(255, 0, 0), -1);
		debugger->add("Intersections Ref.", vis);
	}

	// Compute spacing (in refined coordinates) from adjacent intersection distances.
	const auto N = vGrid.size(); //!< Board size (9,13,19).
	std::vector<double> spacingSamples;
	if (N >= 2 && intersectionsRefined.size() == N * N) {
		spacingSamples.reserve(2 * N * (N - 1));

		for (std::size_t x = 0; x < N; ++x) {
			for (std::size_t y = 0; y + 1 < N; ++y) {
				const double d = cv::norm(intersectionsRefined[x * N + (y + 1)] - intersectionsRefined[x * N + y]);
				if (std::isfinite(d) && d > 0.0)
					spacingSamples.push_back(d);
			}
		}
		for (std::size_t x = 0; x + 1 < N; ++x) {
			for (std::size_t y = 0; y < N; ++y) {
				const double d = cv::norm(intersectionsRefined[(x + 1) * N + y] - intersectionsRefined[x * N + y]);
				if (std::isfinite(d) && d > 0.0)
					spacingSamples.push_back(d);
			}
		}
	}

	const double refinedSpacing = spacingSamples.empty() ? 0.0 : median(spacingSamples);

	// TODO: Rotate nicely horizontally

	if (debugger)
		debugger->endStage();
	std::cout << "\n\n";

	// Assert output: Fail means we missed a validity check earlier.
	assert(vGrid.size() == hGrid.size());         // Grid lines equal.
	assert(intersectionsRefined.size() == N * N); // Intersection count fits grid size.
	assert(N == 9 || N == 13 || N == 19);         // Only valid board sizes.
	assert(!refined.empty());                     // No empty image.
	// TODO: Unit test sanity check: Image (width|height) == N*spacing (up to tolerance. N-1 spacings in Grid + stone buffer)
	return {refined, homographyFinal, intersectionsRefined, refinedSpacing, static_cast<unsigned>(N)};
}

} // namespace tengen::vision::core
