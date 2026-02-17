#include "camera/stoneFinder.hpp"

#include "camera/rectifier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

#include <opencv2/opencv.hpp>

namespace go::camera {

// Stone detection on a rectified Go board.
// Pipeline: BGR -> Lab -> per-intersection circular mean L -> percentile thresholds -> classify -> optional overlay.
StoneResult analyseBoard(const BoardGeometry& geometry, go::camera::DebugVisualizer* debugger) {	
	if (geometry.image.empty()) {
		std::cerr << "Failed to rectify image\n";
		return {false, {}};
	}
	if (geometry.intersections.empty()) {
		std::cerr << "No intersections provided\n";
		return {false, {}};
	}
	if (debugger) {
		debugger->beginStage("Stone Detection");
		debugger->add("Input", geometry.image);
	}

	// Convert to Lab and extract L channel.
	cv::Mat lab;
	if (geometry.image.channels() == 3) {
		cv::cvtColor(geometry.image, lab, cv::COLOR_BGR2Lab);
	} else if (geometry.image.channels() == 4) {
		cv::Mat bgr;
		cv::cvtColor(geometry.image, bgr, cv::COLOR_BGRA2BGR);
		cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	} else if (geometry.image.channels() == 1) {
		cv::Mat bgr;
		cv::cvtColor(geometry.image, bgr, cv::COLOR_GRAY2BGR);
		cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	} else {
		std::cerr << "Unsupported image channels: " << geometry.image.channels() << "\n";
		if (debugger) debugger->endStage();
		return {false, {}};
	}

	cv::Mat L;
	cv::extractChannel(lab, L, 0);

	// Fixed sampling radius (in pixels). Scale with grid spacing if available.
	const int radius = [&]() {
		int r = 6;
		if (std::isfinite(geometry.spacing) && geometry.spacing > 0.0) {
			r = static_cast<int>(std::lround(geometry.spacing * 0.23));
		}
		r = std::max(r, 2);
		r = std::min(r, 30);
		return r;
	}();

	// Precompute integer offsets for a filled circle mask (for speed).
	std::vector<cv::Point> circleOffsets;
	circleOffsets.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
	const int r2 = radius * radius;
	for (int dy = -radius; dy <= radius; ++dy) {
		for (int dx = -radius; dx <= radius; ++dx) {
			if ((dx * dx + dy * dy) <= r2) {
				circleOffsets.emplace_back(dx, dy);
			}
		}
	}

	// Compute mean L around each intersection.
	const int rows = L.rows;
	const int cols = L.cols;

	std::vector<float> meanL;
	meanL.reserve(geometry.intersections.size());
	std::vector<uint8_t> isValid;
	isValid.reserve(geometry.intersections.size());
	std::vector<float> distribution;
	distribution.reserve(geometry.intersections.size());

	for (const auto& p : geometry.intersections) {
		const int cx = static_cast<int>(std::lround(p.x));
		const int cy = static_cast<int>(std::lround(p.y));

		std::uint32_t sum = 0;
		std::uint32_t count = 0;
		for (const auto& off : circleOffsets) {
			const int x = cx + off.x;
			const int y = cy + off.y;
			if (x < 0 || x >= cols || y < 0 || y >= rows) {
				continue;
			}
			sum += static_cast<std::uint32_t>(L.ptr<std::uint8_t>(y)[x]);
			++count;
		}

		if (count == 0) {
			meanL.push_back(0.0f);
			isValid.push_back(static_cast<uint8_t>(0u));
			continue;
		}

		const float m = static_cast<float>(sum) / static_cast<float>(count);
		meanL.push_back(m);
		isValid.push_back(static_cast<uint8_t>(1u));
		distribution.push_back(m);
	}

	if (distribution.empty()) {
		if (debugger) debugger->endStage();
		return {false, {}};
	}

	// Robust thresholds:
	// The old percentile-as-threshold approach forces a fixed fraction of intersections into Black/White,
	// which creates false positives when there are few/no stones. Instead, treat stones as outliers in L.
	std::vector<float> sortedL = distribution;
	std::sort(sortedL.begin(), sortedL.end());
	const std::size_t n = sortedL.size();
	const float medianL = sortedL[(n - 1) / 2];

	std::vector<float> absDev;
	absDev.reserve(n);
	for (float v : sortedL) {
		absDev.push_back(std::abs(v - medianL));
	}
	std::sort(absDev.begin(), absDev.end());
	const float mad = absDev[(n - 1) / 2];
	const float robustStd = std::max(1.0f, 1.4826f * mad);

	static constexpr float MIN_ABS_CONTRAST_L = 24.0f; // Lab L in [0..255]
	static constexpr float K_SIGMA = 3.5f;
	const float delta = std::max(MIN_ABS_CONTRAST_L, K_SIGMA * robustStd);
	const float blackThreshold = medianL - delta;
	const float whiteThreshold = medianL + delta;

	// Classify each intersection and return only detected stones (non-empty).
	std::vector<StoneState> stones;
	stones.reserve(geometry.intersections.size());

	cv::Mat vis;
	if (debugger) {
		vis = geometry.image.clone();
	}

	for (std::size_t i = 0; i < geometry.intersections.size(); ++i) {
		if (isValid[i] == 0u) {
			continue;
		}

		const float l = meanL[i];
		if (l <= blackThreshold) {
			stones.push_back(StoneState::Black);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], radius, cv::Scalar(0, 0, 0), 2);
			}
		} else if (l >= whiteThreshold) {
			stones.push_back(StoneState::White);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], radius, cv::Scalar(255, 0, 0), 2);
			}
		}
	}

	if (debugger) {
		debugger->add("Stone Overlay", vis);
		debugger->endStage();
	}

	return {true, std::move(stones)};
}

}
