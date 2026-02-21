#include "camera/debugVisualizer.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

#include <opencv2/opencv.hpp>

namespace go::vision::core {

void DebugVisualizer::setInteractive(bool interactive, unsigned displayTimeMs) {
	m_interactive = interactive;
	m_displayTime = displayTimeMs;
}

void DebugVisualizer::beginStage(std::string name) {
	if (m_hasActiveStage) {
		endStage();
	}
	m_hasActiveStage    = true;
	m_currentStage.name = std::move(name);
}

void DebugVisualizer::endStage() {
	if (!m_hasActiveStage) {
		return;
	}

	m_stages.emplace_back(std::move(m_currentStage));
	m_currentStage   = DebugStage{};
	m_hasActiveStage = false;
}

void DebugVisualizer::add(std::string name, const cv::Mat& img) {
	if (!m_hasActiveStage) {
		assert(false); // Start a stage first.
		return;
	}

	m_currentStage.images.push_back(DebugStep{std::move(name), img.clone()});

	if (m_interactive) {
		cv::Mat vis = toBgr8U(img); // Normalise the image for displaying.
		cv::imshow("Debug", vis);
		cv::waitKey(m_displayTime);
		cv::destroyWindow("Debug");
	}
}
void DebugVisualizer::clear() {
	m_stages.clear();
	m_currentStage   = DebugStage{};
	m_hasActiveStage = false;
}

cv::Mat DebugVisualizer::buildMosaic() {
	static constexpr int DEFAULT_TILE_W = 360;
	static constexpr int STAGE_HEADER_H = 34;
	static constexpr int TILE_LABEL_H   = 28;
	static constexpr int TILE_PAD       = 4;
	static constexpr int MAX_MOSAIC_W   = 2000;
	static constexpr int MAX_MOSAIC_H   = 3000;

	static const cv::Scalar BG(20, 20, 20);
	static const cv::Scalar HEADER_BG(0, 0, 0);
	static const cv::Scalar HEADER_FG(255, 255, 255);

	if (m_hasActiveStage) {
		endStage();
	}

	if (m_stages.empty()) {
		return {};
	}

	size_t maxSteps = 0;
	for (const auto& stage: m_stages) {
		maxSteps = std::max(maxSteps, stage.images.size());
	}
	if (maxSteps == 0) {
		return {};
	}

	const int cols = static_cast<int>(m_stages.size());
	int tileW      = DEFAULT_TILE_W;
	tileW          = std::min(tileW, std::max(1, MAX_MOSAIC_W / std::max(1, cols)));
	tileW          = std::min(tileW, std::max(1, (MAX_MOSAIC_H - STAGE_HEADER_H) / static_cast<int>(maxSteps)));

	const int tileH   = tileW;
	const int mosaicW = tileW * cols;
	const int mosaicH = STAGE_HEADER_H + static_cast<int>(maxSteps) * tileH;

	cv::Mat mosaic(mosaicH, mosaicW, CV_8UC3, BG);

	// Headers: one stage per column.
	for (int c = 0; c < cols; ++c) {
		const auto& stage = m_stages[static_cast<size_t>(c)];

		cv::Mat header = mosaic(cv::Rect(c * tileW, 0, tileW, STAGE_HEADER_H));
		cv::rectangle(header, cv::Rect(0, 0, header.cols, header.rows), HEADER_BG, cv::FILLED);
		const std::string stageName  = stage.name.empty() ? "Stage " + std::to_string(c + 1) : stage.name;
		const std::string headerText = stageName + " (" + std::to_string(stage.images.size()) + ")";
		cv::putText(header, headerText, cv::Point(8, STAGE_HEADER_H - 10), cv::FONT_HERSHEY_SIMPLEX, 0.75, HEADER_FG, 1, cv::LINE_AA);
	}

	// Tiles: one row per step index, blank if a stage has fewer steps.
	for (size_t r = 0; r < maxSteps; ++r) {
		const int y = STAGE_HEADER_H + static_cast<int>(r) * tileH;
		for (int c = 0; c < cols; ++c) {
			const auto& stage = m_stages[static_cast<size_t>(c)];
			if (r >= stage.images.size()) {
				continue;
			}

			const auto& step = stage.images[r];
			cv::Mat cell     = mosaic(cv::Rect(c * tileW, y, tileW, tileH));

			// Label bar (separate from image so it doesn't cover content).
			cv::rectangle(cell, cv::Rect(0, 0, cell.cols, TILE_LABEL_H), HEADER_BG, cv::FILLED);
			cv::putText(cell, step.name, cv::Point(TILE_PAD, 20), cv::FONT_HERSHEY_SIMPLEX, 0.55, HEADER_FG, 1, cv::LINE_AA);

			if (step.image.empty() || step.image.cols <= 0 || step.image.rows <= 0) {
				continue;
			}

			const int availW = std::max(1, tileW - 2 * TILE_PAD);
			const int availH = std::max(1, tileH - TILE_LABEL_H - 2 * TILE_PAD);

			cv::Mat vis = toBgr8U(step.image);
			const double scale =
			        std::min(static_cast<double>(availW) / static_cast<double>(vis.cols), static_cast<double>(availH) / static_cast<double>(vis.rows));
			const int w             = std::max(1, std::min(availW, static_cast<int>(std::lround(vis.cols * scale))));
			const int h             = std::max(1, std::min(availH, static_cast<int>(std::lround(vis.rows * scale))));
			const int interpolation = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_LINEAR;

			cv::Mat resized;
			cv::resize(vis, resized, cv::Size(w, h), 0.0, 0.0, interpolation);

			const int x0 = TILE_PAD + (availW - w) / 2;
			const int y0 = TILE_LABEL_H + TILE_PAD + (availH - h) / 2;
			resized.copyTo(cell(cv::Rect(x0, y0, w, h)));
		}
	}

	return mosaic;
}

cv::Mat DebugVisualizer::toBgr8U(const cv::Mat& in) {
	cv::Mat out;

	// normalize depth to 8U for visualization
	if (in.depth() != CV_8U) {
		double minV = 0.0, maxV = 0.0;
		cv::minMaxLoc(in.reshape(1), &minV, &maxV);
		if (maxV - minV < 1e-9) {
			in.convertTo(out, CV_8U);
		} else {
			cv::Mat tmp;
			in.convertTo(tmp, CV_32F);
			tmp = (tmp - (float)minV) * (255.0f / (float)(maxV - minV));
			tmp.convertTo(out, CV_8U);
		}
	} else {
		out = in;
	}

	// convert to 3-channel BGR
	if (out.channels() == 1) {
		cv::cvtColor(out, out, cv::COLOR_GRAY2BGR);
	} else if (out.channels() == 4) {
		cv::cvtColor(out, out, cv::COLOR_BGRA2BGR);
	}
	return out;
}

cv::Mat DebugVisualizer::labelTile(const cv::Mat& tile, const std::string& text) {
	cv::Mat out    = tile.clone();
	const int pad  = 6;
	const int barH = 28;

	cv::rectangle(out, cv::Rect(0, 0, out.cols, barH), cv::Scalar(0, 0, 0), cv::FILLED);
	cv::putText(out, text, cv::Point(pad, 20), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
	return out;
}

} // namespace go::camera
