#include "camera/stoneFinder.hpp"

#include "camera/rectifier.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
	cv::Mat A;
	cv::Mat B;
	cv::extractChannel(lab, A, 1);
	cv::extractChannel(lab, B, 2);

	// Fixed sampling radius (in pixels). Scale with grid spacing if available.
	const int innerRadius = [&]() {
		int r = 6;
		if (std::isfinite(geometry.spacing) && geometry.spacing > 0.0) {
			r = static_cast<int>(std::lround(geometry.spacing * 0.28));
		}
		r = std::max(r, 2);
		r = std::min(r, 30);
		return r;
	}();
	const int bgRadius = std::clamp(innerRadius / 2, 2, 12);

	// Blur to suppress thin grid lines (stones are much larger than line thickness).
	cv::Mat Lblur;
	cv::Mat Ablur;
	cv::Mat Bblur;
	{
		const double sigma = std::clamp(0.15 * static_cast<double>(innerRadius), 1.0, 4.0);
		cv::GaussianBlur(L, Lblur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
		cv::GaussianBlur(A, Ablur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
		cv::GaussianBlur(B, Bblur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	}

	auto makeCircleOffsets = [](int radius) {
		std::vector<cv::Point> offsets;
		offsets.reserve(static_cast<std::size_t>((2 * radius + 1) * (2 * radius + 1)));
		const int r2 = radius * radius;
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				if ((dx * dx + dy * dy) <= r2) {
					offsets.emplace_back(dx, dy);
				}
			}
		}
		return offsets;
	};

	// Precompute integer offsets for filled circle masks (for speed).
	const std::vector<cv::Point> innerOffsets = makeCircleOffsets(innerRadius);
	const std::vector<cv::Point> bgOffsets = (bgRadius == innerRadius) ? innerOffsets : makeCircleOffsets(bgRadius);

	// Sampling helper (mean L in a circle around a point).
	const int rows = Lblur.rows;
	const int cols = Lblur.cols;

	auto sampleMeanL = [&](int cx, int cy, const std::vector<cv::Point>& offsets, float& outMean) -> bool {
		std::uint32_t sum = 0;
		std::uint32_t count = 0;
		for (const auto& off : offsets) {
			const int x = cx + off.x;
			const int y = cy + off.y;
			if (x < 0 || x >= cols || y < 0 || y >= rows) {
				continue;
			}
			sum += static_cast<std::uint32_t>(Lblur.ptr<std::uint8_t>(y)[x]);
			++count;
		}

		if (count == 0) return false;
		outMean = static_cast<float>(sum) / static_cast<float>(count);
		return true;
	};

	auto sampleMeanLab = [&](int cx, int cy, const std::vector<cv::Point>& offsets, float& outL, float& outA, float& outB) -> bool {
		std::uint32_t sumL = 0;
		std::uint32_t sumA = 0;
		std::uint32_t sumB = 0;
		std::uint32_t count = 0;
		for (const auto& off : offsets) {
			const int x = cx + off.x;
			const int y = cy + off.y;
			if (x < 0 || x >= cols || y < 0 || y >= rows) {
				continue;
			}
			sumL += static_cast<std::uint32_t>(Lblur.ptr<std::uint8_t>(y)[x]);
			sumA += static_cast<std::uint32_t>(Ablur.ptr<std::uint8_t>(y)[x]);
			sumB += static_cast<std::uint32_t>(Bblur.ptr<std::uint8_t>(y)[x]);
			++count;
		}

		if (count == 0) return false;
		outL = static_cast<float>(sumL) / static_cast<float>(count);
		outA = static_cast<float>(sumA) / static_cast<float>(count);
		outB = static_cast<float>(sumB) / static_cast<float>(count);
		return true;
	};

	// Compute local-normalized deltaL at each intersection (intersection mean minus local background mean).
	const int bgOffset = [&]() {
		if (std::isfinite(geometry.spacing) && geometry.spacing > 0.0) {
			const int d = static_cast<int>(std::lround(geometry.spacing * 0.48));
			return std::max(d, innerRadius + 2);
		}
		return innerRadius * 2 + 6;
	}();

	std::vector<float> deltaL;
	deltaL.reserve(geometry.intersections.size());
	std::vector<float> chromaSq;
	chromaSq.reserve(geometry.intersections.size());
	std::vector<float> darkFrac;
	darkFrac.reserve(geometry.intersections.size());
	std::vector<float> brightFrac;
	brightFrac.reserve(geometry.intersections.size());
	std::vector<uint8_t> isValid;
	isValid.reserve(geometry.intersections.size());
	std::vector<float> distribution;
	distribution.reserve(geometry.intersections.size());

	for (const auto& p : geometry.intersections) {
		const int cx = static_cast<int>(std::lround(p.x));
		const int cy = static_cast<int>(std::lround(p.y));

		float innerL = 0.0f;
		float innerA = 0.0f;
		float innerB = 0.0f;
		if (!sampleMeanLab(cx, cy, innerOffsets, innerL, innerA, innerB)) {
			deltaL.push_back(0.0f);
			chromaSq.push_back(0.0f);
			darkFrac.push_back(0.0f);
			brightFrac.push_back(0.0f);
			isValid.push_back(static_cast<uint8_t>(0u));
			continue;
		}

		std::array<float, 8> bgVals{};
		int bgCount = 0;
		{
			const int dx = bgOffset;
			const int dy = bgOffset;

			float m = 0.0f;
			// Diagonals
			if (sampleMeanL(cx - dx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx - dx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }

			// Cardinals
			if (sampleMeanL(cx - dx, cy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
		}

		if (bgCount < 2) {
			deltaL.push_back(0.0f);
			chromaSq.push_back(0.0f);
			darkFrac.push_back(0.0f);
			brightFrac.push_back(0.0f);
			isValid.push_back(static_cast<uint8_t>(0u));
			continue;
		}

		std::sort(bgVals.begin(), bgVals.begin() + bgCount);
		const float bgMedian = (bgCount % 2 == 1)
			? bgVals[static_cast<std::size_t>(bgCount / 2)]
			: 0.5f * (bgVals[static_cast<std::size_t>(bgCount / 2 - 1)] + bgVals[static_cast<std::size_t>(bgCount / 2)]);

		const float dMed = innerL - bgMedian;

		deltaL.push_back(dMed);
		const float da = innerA - 128.0f;
		const float db = innerB - 128.0f;
		chromaSq.push_back(da * da + db * db);

		// Support fractions: stones cover most of the ROI with consistently darker/brighter pixels.
		// This helps reject small features like star points or grid artifacts.
		{
			static constexpr float SUPPORT_DELTA = 10.0f;
			std::uint32_t total = 0;
			std::uint32_t dark = 0;
			std::uint32_t bright = 0;
			for (const auto& off : innerOffsets) {
				const int x = cx + off.x;
				const int y = cy + off.y;
				if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
				const float v = static_cast<float>(Lblur.ptr<std::uint8_t>(y)[x]);
				const float diff = v - bgMedian;
				++total;
				if (diff <= -SUPPORT_DELTA) ++dark;
				else if (diff >= SUPPORT_DELTA) ++bright;
			}
			if (total == 0) {
				darkFrac.push_back(0.0f);
				brightFrac.push_back(0.0f);
			} else {
				darkFrac.push_back(static_cast<float>(dark) / static_cast<float>(total));
				brightFrac.push_back(static_cast<float>(bright) / static_cast<float>(total));
			}
		}

		isValid.push_back(static_cast<uint8_t>(1u));
		distribution.push_back(dMed);
	}

	if (distribution.empty()) {
		if (debugger) debugger->endStage();
		return {false, {}};
	}

	// Robust thresholds on deltaL (local-normalized), so illumination gradients don't cause false positives/negatives.
	std::vector<float> sortedD = distribution;
	std::sort(sortedD.begin(), sortedD.end());
	const std::size_t n = sortedD.size();
	const float medianD = sortedD[(n - 1) / 2];

	std::vector<float> absDev;
	absDev.reserve(n);
	for (float v : sortedD) {
		absDev.push_back(std::abs(v - medianD));
	}
	std::sort(absDev.begin(), absDev.end());
	const float mad = absDev[(n - 1) / 2];
	const float robustStd = std::max(1.0f, 1.4826f * mad);

	static constexpr float MIN_ABS_CONTRAST_BLACK = 17.0f; // in Lab L units
	static constexpr float MIN_ABS_CONTRAST_WHITE = 11.0f; // whites have weaker contrast; be more permissive
	static constexpr float K_SIGMA = 3.0f;
	const float noiseThresh = K_SIGMA * robustStd;
	const float threshBlack = std::max(MIN_ABS_CONTRAST_BLACK, noiseThresh);
	const float threshWhite = std::max(MIN_ABS_CONTRAST_WHITE, noiseThresh);
	static constexpr float BLACK_BIAS = 6.0f; // be stricter for black stones; avoids dark artifacts
	const float baseBlack = medianD - (threshBlack + BLACK_BIAS);
	const float baseWhite = medianD + threshWhite;
	const float neighborMargin = std::max(3.0f, 1.2f * robustStd);
	const int N = static_cast<int>(geometry.boardSize);

	auto neighborMedianDelta = [&](int idx) -> float {
		if (N <= 0) return 0.0f;
		const int x = idx / N;
		const int y = idx - x * N;

		std::array<float, 8> vals{};
		int cnt = 0;
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				if (dx == 0 && dy == 0) continue;
				const int nx = x + dx;
				const int ny = y + dy;
				if (nx < 0 || nx >= N || ny < 0 || ny >= N) continue;
				const int nidx = nx * N + ny;
				if (nidx < 0 || static_cast<std::size_t>(nidx) >= deltaL.size()) continue;
				if (isValid[static_cast<std::size_t>(nidx)] == 0u) continue;
				vals[static_cast<std::size_t>(cnt++)] = deltaL[static_cast<std::size_t>(nidx)];
			}
		}
		if (cnt == 0) return deltaL[static_cast<std::size_t>(idx)];

		std::sort(vals.begin(), vals.begin() + cnt);
		return (cnt % 2 == 1)
			? vals[static_cast<std::size_t>(cnt / 2)]
			: 0.5f * (vals[static_cast<std::size_t>(cnt / 2 - 1)] + vals[static_cast<std::size_t>(cnt / 2)]);
	};

	auto computeMetricsAt = [&](int cx, int cy, float& outDeltaL, float& outChromaSq, float& outDarkFrac, float& outBrightFrac) -> bool {
		float innerL = 0.0f;
		float innerA = 0.0f;
		float innerB = 0.0f;
		if (!sampleMeanLab(cx, cy, innerOffsets, innerL, innerA, innerB)) return false;

		std::array<float, 8> bgVals{};
		int bgCount = 0;
		{
			const int dx = bgOffset;
			const int dy = bgOffset;
			float m = 0.0f;
			if (sampleMeanL(cx - dx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx - dx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx - dx, cy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx + dx, cy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx, cy - dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
			if (sampleMeanL(cx, cy + dy, bgOffsets, m)) { bgVals[static_cast<std::size_t>(bgCount++)] = m; }
		}

		if (bgCount < 2) return false;

		std::sort(bgVals.begin(), bgVals.begin() + bgCount);
		const float bgMedian = (bgCount % 2 == 1)
			? bgVals[static_cast<std::size_t>(bgCount / 2)]
			: 0.5f * (bgVals[static_cast<std::size_t>(bgCount / 2 - 1)] + bgVals[static_cast<std::size_t>(bgCount / 2)]);

		outDeltaL = innerL - bgMedian;
		const float da = innerA - 128.0f;
		const float db = innerB - 128.0f;
		outChromaSq = da * da + db * db;

		static constexpr float SUPPORT_DELTA = 10.0f;
		std::uint32_t total = 0;
		std::uint32_t dark = 0;
		std::uint32_t bright = 0;
		for (const auto& off : innerOffsets) {
			const int x = cx + off.x;
			const int y = cy + off.y;
			if (x < 0 || x >= cols || y < 0 || y >= rows) continue;
			const float v = static_cast<float>(Lblur.ptr<std::uint8_t>(y)[x]);
			const float diff = v - bgMedian;
			++total;
			if (diff <= -SUPPORT_DELTA) ++dark;
			else if (diff >= SUPPORT_DELTA) ++bright;
		}
		if (total == 0) {
			outDarkFrac = 0.0f;
			outBrightFrac = 0.0f;
		} else {
			outDarkFrac = static_cast<float>(dark) / static_cast<float>(total);
			outBrightFrac = static_cast<float>(bright) / static_cast<float>(total);
		}
		return true;
	};

	// Classify each intersection and return only detected stones (non-empty).
	std::vector<StoneState> stones;
	stones.reserve(geometry.intersections.size());

	std::vector<std::size_t> detectedIdx;
#ifndef NDEBUG
	const bool debugPrint = (std::getenv("GO_CAMERA_STONE_DEBUG") != nullptr);
	if (debugPrint) detectedIdx.reserve(geometry.intersections.size());
	std::vector<int> debugTrackIdx;
	if (debugPrint) {
		if (const char* s = std::getenv("GO_CAMERA_STONE_DEBUG_TRACK")) {
			int cur = 0;
			bool has = false;
			for (const char* p = s; ; ++p) {
				const char ch = *p;
				if (ch >= '0' && ch <= '9') {
					cur = cur * 10 + (ch - '0');
					has = true;
				} else {
					if (has) debugTrackIdx.push_back(cur);
					cur = 0;
					has = false;
					if (ch == '\0') break;
				}
			}
		}
	}
#else
	const bool debugPrint = false;
#endif

	cv::Mat vis;
	if (debugger) {
		vis = geometry.image.clone();
	}

	for (std::size_t i = 0; i < geometry.intersections.size(); ++i) {
		if (isValid[i] == 0u) {
			continue;
		}

		const int gx = (N > 0) ? (static_cast<int>(i) / N) : 0;
		const int gy = (N > 0) ? (static_cast<int>(i) - gx * N) : 0;
		const bool onEdge = (gx == 0) || (gx == N - 1) || (gy == 0) || (gy == N - 1);
		const bool nearEdge = (gx <= 1) || (gx >= N - 2) || (gy <= 1) || (gy >= N - 2);

		float d = deltaL[i];
		float df = darkFrac[i];
		float bf = brightFrac[i];
		float csq = chromaSq[i];
		static constexpr float MAX_CHROMA_SQ = 30.0f * 30.0f;
		if (csq > MAX_CHROMA_SQ) continue;

		bool candBlack = d <= baseBlack;
		bool candWhite = d >= baseWhite;

		// Refine borderline whites: small grid jitter can move the sampling circle slightly off-center.
		if (!candBlack) {
			static constexpr float WHITE_MIN_SUPPORT_FRAC = 0.40f;
			const float refineRange = onEdge ? 6.0f : 4.5f;
			// Allow a slightly larger refinement window only when the pre-sample already looks "stone-like".
			const float extraRange = (!onEdge && (bf >= 0.35f)) ? 0.6f : 0.0f;
			const float minPreBright = onEdge ? 0.25f : 0.28f;
			const bool needsSupport = candWhite && ((bf < WHITE_MIN_SUPPORT_FRAC) || (bf < df + 0.10f));
			static constexpr float BORDERLINE_MAX_CHROMA_SQ = 10.0f * 10.0f;
			const bool borderline = (!candWhite && !candBlack
				&& (d >= baseWhite - (refineRange + extraRange))
				&& (bf >= minPreBright)
				&& (csq <= BORDERLINE_MAX_CHROMA_SQ));

			if (needsSupport || borderline) {
				const int cx0 = static_cast<int>(std::lround(geometry.intersections[i].x));
				const int cy0 = static_cast<int>(std::lround(geometry.intersections[i].y));

				bool found = false;
				float bestD = d;
				float bestDf = df;
				float bestBf = bf;
				float bestCsq = csq;

				static constexpr int STEP = 2;
				const int extent = onEdge ? 12 : (nearEdge ? 8 : 6);
				for (int oy = -extent; oy <= extent; oy += STEP) {
					for (int ox = -extent; ox <= extent; ox += STEP) {
						if (ox == 0 && oy == 0) continue;
						float rd = 0.0f;
						float rcsq = 0.0f;
						float rdf = 0.0f;
						float rbf = 0.0f;
						if (!computeMetricsAt(cx0 + ox, cy0 + oy, rd, rcsq, rdf, rbf)) continue;
						if (rcsq > MAX_CHROMA_SQ) continue;
						static constexpr float MAX_CHROMA_SQ_WHITE = 20.0f * 20.0f;
						static constexpr float MAX_CHROMA_SQ_WHITE_STRICT = 10.0f * 10.0f;
						const float edgeChromaCap = onEdge ? 150.0f : 200.0f;
						static constexpr float WEAK_WHITE_MARGIN = 3.0f;
						if (rcsq > MAX_CHROMA_SQ_WHITE) continue;
						if (nearEdge && rcsq > edgeChromaCap) continue;
						if (rd < baseWhite + WEAK_WHITE_MARGIN && rcsq > MAX_CHROMA_SQ_WHITE_STRICT) continue;
						static constexpr float MIN_SUPPORT_FRAC = WHITE_MIN_SUPPORT_FRAC;
						if (rbf < MIN_SUPPORT_FRAC) continue;
						if (rbf < rdf + 0.10f) continue;
						if (rd < baseWhite) continue;
						if (!found || rd > bestD) {
							found = true;
							bestD = rd;
							bestDf = rdf;
							bestBf = rbf;
							bestCsq = rcsq;
						}
					}
				}

				if (found) {
					d = bestD;
					df = bestDf;
					bf = bestBf;
					csq = bestCsq;
					candWhite = d >= baseWhite;
				}
			}
		}

		if (!candBlack && !candWhite) continue;

		const float neigh = neighborMedianDelta(static_cast<int>(i));

		if (candBlack && d <= neigh - neighborMargin) {
			static constexpr float MAX_CHROMA_SQ_BLACK = 200.0f;
			if (csq > MAX_CHROMA_SQ_BLACK) continue;
			static constexpr float MIN_SUPPORT_FRAC = 0.35f;
			if (df < MIN_SUPPORT_FRAC) continue;
			if (df < bf + 0.10f) continue;
			stones.push_back(StoneState::Black);
			if (debugPrint) detectedIdx.push_back(i);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], innerRadius, cv::Scalar(0, 0, 0), 2);
			}
		} else if (candWhite && d >= neigh + neighborMargin) {
			static constexpr float MAX_CHROMA_SQ_WHITE = 20.0f * 20.0f;
			static constexpr float MAX_CHROMA_SQ_WHITE_STRICT = 10.0f * 10.0f;
			const float edgeChromaCap = onEdge ? 150.0f : 200.0f;
			static constexpr float WEAK_WHITE_MARGIN = 3.0f;
			if (csq > MAX_CHROMA_SQ_WHITE) continue;
			if (nearEdge && csq > edgeChromaCap) continue;
			if (d < baseWhite + WEAK_WHITE_MARGIN && csq > MAX_CHROMA_SQ_WHITE_STRICT) continue;
			// Near the board edge, empty intersections can look like low-contrast "white stones" (less grid coverage).
			// Be stricter for low-contrast whites near the border, but keep true edge stones (neutral chroma) working.
			{
				static constexpr float EDGE_WEAK_MARGIN = 2.0f;
				static constexpr float EDGE_MAX_CHROMA_SQ_WEAK = 70.0f;
				if (nearEdge && (d < baseWhite + EDGE_WEAK_MARGIN) && (csq > EDGE_MAX_CHROMA_SQ_WEAK)) continue;
			}
			static constexpr float MIN_SUPPORT_FRAC = 0.40f;
			if (bf < MIN_SUPPORT_FRAC) continue;
			if (bf < df + 0.10f) continue;
			stones.push_back(StoneState::White);
			if (debugPrint) detectedIdx.push_back(i);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], innerRadius, cv::Scalar(255, 0, 0), 2);
			}
		}
	}

#ifndef NDEBUG
	if (debugPrint) {
		std::cout << "[StoneDebug] N=" << geometry.boardSize
		          << " stones=" << stones.size()
		          << " baseBlack=" << baseBlack
		          << " baseWhite=" << baseWhite
		          << " neighborMargin=" << neighborMargin
		          << " innerR=" << innerRadius
		          << " bgR=" << bgRadius
		          << " bgOffset=" << bgOffset
		          << "\n";
		for (std::size_t idx : detectedIdx) {
			const int x = static_cast<int>(idx) / N;
			const int y = static_cast<int>(idx) - x * N;
			const float d = deltaL[idx];
			std::cout << "  idx=" << idx
			          << " (" << x << "," << y << ")"
			          << " d=" << d
			          << " neigh=" << neighborMedianDelta(static_cast<int>(idx))
			          << " darkFrac=" << darkFrac[idx]
			          << " brightFrac=" << brightFrac[idx]
			          << " chromaSq=" << chromaSq[idx]
			          << "\n";
		}
		for (int tidx : debugTrackIdx) {
			if (tidx < 0 || static_cast<std::size_t>(tidx) >= deltaL.size()) continue;
			const std::size_t idx = static_cast<std::size_t>(tidx);
			const int x = static_cast<int>(idx) / N;
			const int y = static_cast<int>(idx) - x * N;
			const float d = deltaL[idx];
			const float neigh = neighborMedianDelta(static_cast<int>(idx));
			const bool candBlack = d <= baseBlack;
			const bool candWhite = d >= baseWhite;
			std::cout << "  [track] idx=" << idx
			          << " (" << x << "," << y << ")"
			          << " d=" << d
			          << " neigh=" << neigh
			          << " candB=" << static_cast<int>(candBlack)
			          << " candW=" << static_cast<int>(candWhite)
			          << " darkFrac=" << darkFrac[idx]
			          << " brightFrac=" << brightFrac[idx]
			          << " chromaSq=" << chromaSq[idx]
			          << "\n";
		}
	}
#endif

	if (debugger) {
		debugger->add("Stone Overlay", vis);
		debugger->endStage();
	}

	return {true, std::move(stones)};
}

}
