#include "camera/stoneFinder.hpp"

#include "camera/rectifier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

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

/*! Detect stones on a rectified Go board.
 *
 * Major steps:
 *  1) Convert the input image to Lab and blur channels to suppress thin grid lines.
 *  2) For each intersection: sample mean Lab in a small filled circle and estimate local background L
 *     from 8 surrounding circles; compute deltaL and support fractions.
 *  3) Build robust global thresholds on deltaL using median + MAD (noise-adaptive).
 *  4) Classify each intersection using thresholds + local neighbor contrast; optionally refine borderline
 *     white candidates by searching a small pixel neighborhood (compensates for grid jitter).
 *
 * \param [in]     geometry Rectified board image + intersection points (in rectified coordinates).
 * \param [in,out] debugger Optional visualizer (may be nullptr). When present, an overlay is drawn.
 * \return         StoneResult with success flag and a list of detected stones (only non-empty entries are returned).
 */
StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger) {
	// 0. Validate inputs.
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

	// 1. Convert to Lab (OpenCV uses 8-bit L,a,b; L in [0,255], a/b centered at 128).
	cv::Mat lab; // Lab image (CV_8UC3)
	if (geometry.image.channels() == 3) {
		cv::cvtColor(geometry.image, lab, cv::COLOR_BGR2Lab);
	} else if (geometry.image.channels() == 4) {
		cv::Mat bgr; // temporary 3-channel conversion (BGRA -> BGR)
		cv::cvtColor(geometry.image, bgr, cv::COLOR_BGRA2BGR);
		cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	} else if (geometry.image.channels() == 1) {
		cv::Mat bgr; // temporary 3-channel conversion (gray -> BGR)
		cv::cvtColor(geometry.image, bgr, cv::COLOR_GRAY2BGR);
		cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
	} else {
		std::cerr << "Unsupported image channels: " << geometry.image.channels() << "\n";
		if (debugger)
			debugger->endStage();
		return {false, {}};
	}

	// 1.1 Split Lab channels (we use L for brightness and a/b for chroma gating).
	cv::Mat L; // lightness channel (CV_8U)
	cv::Mat A; // a channel (CV_8U), neutral ~ 128
	cv::Mat B; // b channel (CV_8U), neutral ~ 128
	cv::extractChannel(lab, L, 0);
	cv::extractChannel(lab, A, 1);
	cv::extractChannel(lab, B, 2);

	// 2. Choose sampling radii (in pixels). Scale with grid spacing so the ROI tracks stone size.
	const int innerRadius = [&]() {
		int r = 6; // fallback radius (px)
		if (std::isfinite(geometry.spacing) && geometry.spacing > 0.0) {
			r = static_cast<int>(std::lround(geometry.spacing * 0.28)); // Magic 28% to get all tests working...
		}
		r = std::max(r, 2);  // lower clamp: need at least a few pixels
		r = std::min(r, 30); // upper clamp: keep runtime bounded for very large images
		return r;
	}();
	const int bgRadius = std::clamp(innerRadius / 2, 2, 12); //!< Background sample circle radius (px)

	// 3. Blur to suppress thin grid lines (stones are much larger than line thickness).
	cv::Mat Lblur; // blurred lightness
	cv::Mat Ablur; // blurred a
	cv::Mat Bblur; // blurred b
	{
		const double sigma = std::clamp(0.15 * static_cast<double>(innerRadius), 1.0, 4.0); //!< blur scale (px)
		cv::GaussianBlur(L, Lblur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
		cv::GaussianBlur(A, Ablur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
		cv::GaussianBlur(B, Bblur, cv::Size(), sigma, sigma, cv::BORDER_REPLICATE);
	}

	// 4. Precompute filled-circle pixel offsets (for speed; avoids per-sample mask allocation).
	auto makeCircleOffsets = [](int radius) {
		std::vector<cv::Point> offsets; // integer (dx,dy) offsets inside a filled circle
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

	const std::vector<cv::Point> innerOffsets = makeCircleOffsets(innerRadius);                                         //!< ROI offsets
	const std::vector<cv::Point> bgOffsets    = (bgRadius == innerRadius) ? innerOffsets : makeCircleOffsets(bgRadius); //!< Background ROI offsets

	// 5. Define fast sampling helpers (bounds-safe: out-of-image pixels are skipped).
	const int rows = Lblur.rows; //!< Image height
	const int cols = Lblur.cols; //!< Image width

	auto sampleMeanL = [&](int cx, int cy, const std::vector<cv::Point>& offsets, float& outMean) -> bool {
		std::uint32_t sum   = 0;         //!< Sum of L values
		std::uint32_t count = 0;         //!< Number of in-bounds pixels
		
		for (const auto& off: offsets) {
			const int x = cx + off.x;    //!< Pixel x
			const int y = cy + off.y;    //!< Pixel y
			if (x < 0 || x >= cols || y < 0 || y >= rows) {
				continue;
			}
			sum += static_cast<std::uint32_t>(Lblur.ptr<std::uint8_t>(y)[x]);
			++count;
		}

		if (count == 0){
			return false;
		}
		outMean = static_cast<float>(sum) / static_cast<float>(count);
		return true;
	};

	auto sampleMeanLab = [&](int cx, int cy, const std::vector<cv::Point>& offsets, float& outL, float& outA, float& outB) -> bool {
		std::uint32_t sumL  = 0;         //!< Sum of L values
		std::uint32_t sumA  = 0;         //!< Sum of a values
		std::uint32_t sumB  = 0;         //!< Sum of b values
		std::uint32_t count = 0;         //!< Number of in-bounds pixels
		
		for (const auto& off: offsets) {
			const int x = cx + off.x;    //!< Pixel x
			const int y = cy + off.y;    //!< Pixel y
			if (x < 0 || x >= cols || y < 0 || y >= rows) {
				continue;
			}

			sumL += static_cast<std::uint32_t>(Lblur.ptr<std::uint8_t>(y)[x]);
			sumA += static_cast<std::uint32_t>(Ablur.ptr<std::uint8_t>(y)[x]);
			sumB += static_cast<std::uint32_t>(Bblur.ptr<std::uint8_t>(y)[x]);
			++count;
		}

		if (count == 0){
			return false;
		}
		outL = static_cast<float>(sumL) / static_cast<float>(count);
		outA = static_cast<float>(sumA) / static_cast<float>(count);
		outB = static_cast<float>(sumB) / static_cast<float>(count);

		return true;
	};

	// 6. Compute per-intersection metrics:
	//    - deltaL: local-normalized lightness (intersection mean minus local background median),
	//    - chromaSq: (a,b) distance to neutral (reject colored artifacts),
	//    - darkFrac/brightFrac: fraction of ROI pixels that consistently support a stone hypothesis.
	const int bgOffset = [&]() {
		if (std::isfinite(geometry.spacing) && geometry.spacing > 0.0) {
			const int d = static_cast<int>(std::lround(geometry.spacing * 0.48)); //!< Background sample distance (px)
			return std::max(d, innerRadius + 2);                                  // keep background samples outside the stone ROI
		}
		return innerRadius * 2 + 6; // fallback distance (px)
	}();

	std::vector<float> deltaL; // per-intersection deltaL = L_inner - median(L_bg)
	deltaL.reserve(geometry.intersections.size());

	std::vector<float> chromaSq; // per-intersection chroma^2 = (a-128)^2 + (b-128)^2
	chromaSq.reserve(geometry.intersections.size());

	std::vector<float> darkFrac; // per-intersection fraction of ROI pixels significantly darker than bg
	darkFrac.reserve(geometry.intersections.size());

	std::vector<float> brightFrac; // per-intersection fraction of ROI pixels significantly brighter than bg
	brightFrac.reserve(geometry.intersections.size());

	std::vector<uint8_t> isValid; // per-intersection validity flag (sampling succeeded)
	isValid.reserve(geometry.intersections.size());

	std::vector<float> distribution; // valid deltaL values (for robust threshold estimation)
	distribution.reserve(geometry.intersections.size());

	for (const auto& p: geometry.intersections) {
		const int cx = static_cast<int>(std::lround(p.x)); //!< Center x (px)
		const int cy = static_cast<int>(std::lround(p.y)); //!< Center y (px)

		float innerL = 0.0f; //!< Mean L inside ROI
		float innerA = 0.0f; //!< Mean a inside ROI
		float innerB = 0.0f; //!< Mean b inside ROI
		if (!sampleMeanLab(cx, cy, innerOffsets, innerL, innerA, innerB)) {
			// Not enough in-bounds pixels (intersection too far outside the image).
			deltaL.push_back(0.0f);
			chromaSq.push_back(0.0f);
			darkFrac.push_back(0.0f);
			brightFrac.push_back(0.0f);
			isValid.push_back(static_cast<uint8_t>(0u));
			continue;
		}

		std::array<float, 8> bgVals{}; //!< Background L means from 8 surrounding sample circles
		int bgCount = 0;               //!< How many background samples succeeded (in-bounds)
		{
			const int dx = bgOffset; //!< Background sample offset in x (px)
			const int dy = bgOffset; //!< Background sample offset in y (px)
			float m = 0.0f; //!< Temporary mean-L accumulator

			// Diagonals (reduce directional bias).
			if (sampleMeanL(cx - dx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx - dx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}

			// Cardinals.
			if (sampleMeanL(cx - dx, cy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
		}

		if (bgCount < 2) {
			// Without a stable local background estimate, deltaL is unreliable -> invalidate.
			deltaL.push_back(0.0f);
			chromaSq.push_back(0.0f);
			darkFrac.push_back(0.0f);
			brightFrac.push_back(0.0f);
			isValid.push_back(static_cast<uint8_t>(0u));
			continue;
		}

		std::sort(bgVals.begin(), bgVals.begin() + bgCount); // median over available samples
		const float bgMedian = (bgCount % 2 == 1) ? bgVals[static_cast<std::size_t>(bgCount / 2)]
		                                          : 0.5f * (bgVals[static_cast<std::size_t>(bgCount / 2 - 1)] + bgVals[static_cast<std::size_t>(bgCount / 2)]);

		const float dMed = innerL - bgMedian; //!< Local-normalized contrast (stone vs background)

		deltaL.push_back(dMed);
		const float da = innerA - 128.0f; //!< A offset from neutral
		const float db = innerB - 128.0f; //!< B offset from neutral
		chromaSq.push_back(da * da + db * db);

		// Support fractions: stones cover most of the ROI with consistently darker/brighter pixels.
		// This helps reject small features like star points or grid artifacts.
		{
			static constexpr float SUPPORT_DELTA = 10.0f; //!< L threshold (Lab units) relative to bgMedian
			std::uint32_t total                  = 0;     //!< ROI pixel count
			std::uint32_t dark                   = 0;     //!< #pixels with diff <= -SUPPORT_DELTA
			std::uint32_t bright                 = 0;     //!< #pixels with diff >= +SUPPORT_DELTA
			
			// off = ROI offset inside the filled circle
			for (const auto& off: innerOffsets) {
				const int x = cx + off.x;                 //!< Pixel x
				const int y = cy + off.y;                 //!< Pixel y
				if (x < 0 || x >= cols || y < 0 || y >= rows){
					continue;
				}
				const float v    = static_cast<float>(Lblur.ptr<std::uint8_t>(y)[x]); //!< L at pixel
				const float diff = v - bgMedian;                                      //!< L - bgMedian
				++total;

				if (diff <= -SUPPORT_DELTA){
					++dark;
				} else if (diff >= SUPPORT_DELTA) {
					++bright;
				}
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

	// 7. Robust global thresholds on deltaL (local-normalized).
	//    Using median + MAD adapts to image noise and avoids brittle fixed thresholds.
	std::vector<float> sortedD = distribution; // copy to sort (deltaL distribution)
	std::sort(sortedD.begin(), sortedD.end());
	const std::size_t n = sortedD.size();       //!< Number of valid intersections
	const float medianD = sortedD[(n - 1) / 2]; //!< Median deltaL

	std::vector<float> absDev; //!< Absolute deviations from median
	absDev.reserve(n);
	for (float v: sortedD) { // v = one deltaL sample
		absDev.push_back(std::abs(v - medianD));
	}
	std::sort(absDev.begin(), absDev.end());
	const float mad       = absDev[(n - 1) / 2];           //!< median absolute deviation
	const float robustStd = std::max(1.0f, 1.4826f * mad); //!< MAD->sigma (Gaussian) with floor

	static constexpr float MIN_ABS_CONTRAST_BLACK = 17.0f; //!< Minimum |deltaL| for black stones (Lab L units)
	static constexpr float MIN_ABS_CONTRAST_WHITE = 11.0f; //!< Minimum |deltaL| for white stones (weaker contrast)
	static constexpr float K_SIGMA                = 3.0f;  //!< Noise multiplier for thresholding

	const float noiseThresh = K_SIGMA * robustStd;                           //!< Noise-based contrast threshold
	const float threshBlack = std::max(MIN_ABS_CONTRAST_BLACK, noiseThresh); //!< Black contrast threshold
	const float threshWhite = std::max(MIN_ABS_CONTRAST_WHITE, noiseThresh); //!< White contrast threshold

	static constexpr float BLACK_BIAS = 6.0f;                                 //!< Be stricter for black stones; avoids dark artifacts
	const float baseBlack             = medianD - (threshBlack + BLACK_BIAS); //!< Candidate-black if deltaL <= baseBlack
	const float baseWhite             = medianD + threshWhite;                //!< Candidate-white if deltaL >= baseWhite

	const float neighborMargin = std::max(3.0f, 1.2f * robustStd);     //!< Local contrast margin vs neighbor median
	const int N                = static_cast<int>(geometry.boardSize); //!< Board size (grid is N x N)

	// 8. Helper: local neighbor median of deltaL (acts as a local baseline; helps suppress illumination drift).
	auto neighborMedianDelta = [&](int idx) -> float { // idx = flattened intersection index [0, N*N)
		if (N <= 0)
			return 0.0f;
		const int x = idx / N;     // grid x-index
		const int y = idx - x * N; // grid y-index

		std::array<float, 8> vals{};           // neighbor deltaL values (up to 8)
		int cnt = 0;                           // number of valid neighbors
		for (int dx = -1; dx <= 1; ++dx) {     // dx = neighbor x-offset in {-1,0,1}
			for (int dy = -1; dy <= 1; ++dy) { // dy = neighbor y-offset in {-1,0,1}
				if (dx == 0 && dy == 0)
					continue;          // skip self
				const int nx = x + dx; // neighbor x
				const int ny = y + dy; // neighbor y
				if (nx < 0 || nx >= N || ny < 0 || ny >= N)
					continue;
				const int nidx = nx * N + ny; // neighbor flattened index
				if (nidx < 0 || static_cast<std::size_t>(nidx) >= deltaL.size())
					continue;
				if (isValid[static_cast<std::size_t>(nidx)] == 0u)
					continue;
				vals[static_cast<std::size_t>(cnt++)] = deltaL[static_cast<std::size_t>(nidx)];
			}
		}
		if (cnt == 0)
			return deltaL[static_cast<std::size_t>(idx)]; // fallback: use self

		std::sort(vals.begin(), vals.begin() + cnt);
		return (cnt % 2 == 1) ? vals[static_cast<std::size_t>(cnt / 2)]
		                      : 0.5f * (vals[static_cast<std::size_t>(cnt / 2 - 1)] + vals[static_cast<std::size_t>(cnt / 2)]);
	};

	// 9. Helper: recompute metrics at an offset position (used by the refinement search).
	auto computeMetricsAt = [&](int cx, int cy, float& outDeltaL, float& outChromaSq, float& outDarkFrac, float& outBrightFrac) -> bool {
		float innerL = 0.0f; // mean L at (cx,cy)
		float innerA = 0.0f; // mean a at (cx,cy)
		float innerB = 0.0f; // mean b at (cx,cy)
		if (!sampleMeanLab(cx, cy, innerOffsets, innerL, innerA, innerB))
			return false;

		std::array<float, 8> bgVals{}; // background L means from 8 surrounding samples
		int bgCount = 0;               // number of bg samples used
		{
			const int dx = bgOffset; // background sample offset in x (px)
			const int dy = bgOffset; // background sample offset in y (px)
			float m      = 0.0f;     // temporary mean-L accumulator
			if (sampleMeanL(cx - dx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx - dx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx - dx, cy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx + dx, cy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx, cy - dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
			if (sampleMeanL(cx, cy + dy, bgOffsets, m)) {
				bgVals[static_cast<std::size_t>(bgCount++)] = m;
			}
		}

		if (bgCount < 2)
			return false;

		std::sort(bgVals.begin(), bgVals.begin() + bgCount);
		const float bgMedian = (bgCount % 2 == 1) ? bgVals[static_cast<std::size_t>(bgCount / 2)]
		                                          : 0.5f * (bgVals[static_cast<std::size_t>(bgCount / 2 - 1)] + bgVals[static_cast<std::size_t>(bgCount / 2)]);

		outDeltaL      = innerL - bgMedian; // local-normalized contrast (stone vs background)
		const float da = innerA - 128.0f;   // a offset from neutral
		const float db = innerB - 128.0f;   // b offset from neutral
		outChromaSq    = da * da + db * db; // chroma^2 distance to neutral

		static constexpr float SUPPORT_DELTA = 10.0f; // L threshold (Lab units) relative to bgMedian
		std::uint32_t total                  = 0;     // ROI pixel count
		std::uint32_t dark                   = 0;     // #pixels with diff <= -SUPPORT_DELTA
		std::uint32_t bright                 = 0;     // #pixels with diff >= +SUPPORT_DELTA
		for (const auto& off: innerOffsets) {         // off = ROI offset inside the filled circle
			const int x = cx + off.x;                 // pixel x
			const int y = cy + off.y;                 // pixel y
			if (x < 0 || x >= cols || y < 0 || y >= rows)
				continue;
			const float v    = static_cast<float>(Lblur.ptr<std::uint8_t>(y)[x]); // L at pixel
			const float diff = v - bgMedian;                                      // L - bgMedian
			++total;
			if (diff <= -SUPPORT_DELTA)
				++dark;
			else if (diff >= SUPPORT_DELTA)
				++bright;
		}
		if (total == 0) {
			outDarkFrac   = 0.0f;
			outBrightFrac = 0.0f;
		} else {
			outDarkFrac   = static_cast<float>(dark) / static_cast<float>(total);
			outBrightFrac = static_cast<float>(bright) / static_cast<float>(total);
		}
		return true;
	};

	// 10. Classify each intersection.
	//     NOTE: We return only detected stones (Black/White). Empty intersections are skipped.
	std::vector<StoneState> stones; // output list of detected stones (order = detection order, not board order)
	stones.reserve(geometry.intersections.size());

	std::vector<std::size_t> detectedIdx; // debug-only: intersection indices that were classified as stones
#ifndef NDEBUG
	const bool debugPrint = (std::getenv("GO_CAMERA_STONE_DEBUG") != nullptr); // enable stdout debug prints
	if (debugPrint)
		detectedIdx.reserve(geometry.intersections.size());
	std::vector<int> debugTrackIdx; // indices to track even if not detected
	if (debugPrint) {
		if (const char* s = std::getenv("GO_CAMERA_STONE_DEBUG_TRACK")) {
			int cur  = 0;                   // currently parsed integer
			bool has = false;               // currently parsing digits?
			for (const char* p = s;; ++p) { // parse comma/space separated ints
				const char ch = *p;         // current character
				if (ch >= '0' && ch <= '9') {
					cur = cur * 10 + (ch - '0');
					has = true;
				} else {
					if (has)
						debugTrackIdx.push_back(cur);
					cur = 0;
					has = false;
					if (ch == '\0')
						break;
				}
			}
		}
	}
#else
	const bool debugPrint = false;
#endif

	cv::Mat vis; // optional visualization overlay
	if (debugger) {
		vis = geometry.image.clone();
	}

	// 10.1 Per-intersection classification.
	for (std::size_t i = 0; i < geometry.intersections.size(); ++i) { // i = flattened intersection index
		if (isValid[i] == 0u) {
			continue;
		}

		const int gx        = (N > 0) ? (static_cast<int>(i) / N) : 0;                  // grid x-index
		const int gy        = (N > 0) ? (static_cast<int>(i) - gx * N) : 0;             // grid y-index
		const bool onEdge   = (gx == 0) || (gx == N - 1) || (gy == 0) || (gy == N - 1); // outermost ring
		const bool nearEdge = (gx <= 1) || (gx >= N - 2) || (gy <= 1) || (gy >= N - 2); // 2-wide border

		float d   = deltaL[i];     // deltaL at this intersection
		float df  = darkFrac[i];   // dark support fraction
		float bf  = brightFrac[i]; // bright support fraction
		float csq = chromaSq[i];   // chroma^2 at this intersection

		static constexpr float MAX_CHROMA_SQ = 30.0f * 30.0f; // hard reject extreme color (strong wood/ink artifacts)
		if (csq > MAX_CHROMA_SQ)
			continue;

		bool candBlack = d <= baseBlack; // coarse black candidate
		bool candWhite = d >= baseWhite; // coarse white candidate

		// 10.2 Optional refinement for white stones:
		// Whites can be low-contrast and small grid/intersection jitter can move the ROI off-center.
		// We search a small pixel neighborhood and keep the best-supported white hypothesis.
		if (!candBlack) {
			static constexpr float WHITE_MIN_SUPPORT_FRAC = 0.40f;                                    // whites should have strong bright support
			const float refineRange                       = onEdge ? 6.0f : 4.5f;                     // deltaL window to trigger refinement
			const float extraRange                        = (!onEdge && (bf >= 0.35f)) ? 0.6f : 0.0f; // widen trigger if already bright-supported
			const float minPreBright                      = onEdge ? 0.25f : 0.28f;                   // minimum bf to consider borderline refinement
			const bool needsSupport                       = candWhite && ((bf < WHITE_MIN_SUPPORT_FRAC) || (bf < df + 0.10f)); // candWhite but weak support

			// For borderline (not-yet-candidate) whites, only refine if chroma is already close to neutral.
			// This avoids spending time on colored artifacts that cannot become valid whites anyway.
			static constexpr float BORDERLINE_MAX_CHROMA_SQ = 10.0f * 10.0f; // chroma cap for borderline-white refinement trigger
			const bool borderline =
			        (!candWhite && !candBlack && (d >= baseWhite - (refineRange + extraRange)) && (bf >= minPreBright) && (csq <= BORDERLINE_MAX_CHROMA_SQ));

			if (needsSupport || borderline) {
				const int cx0 = static_cast<int>(std::lround(geometry.intersections[i].x)); // initial center x
				const int cy0 = static_cast<int>(std::lround(geometry.intersections[i].y)); // initial center y

				bool found    = false; // whether we found a refined white candidate
				float bestD   = d;     // best refined deltaL
				float bestDf  = df;    // best refined dark support
				float bestBf  = bf;    // best refined bright support
				float bestCsq = csq;   // best refined chroma^2

				static constexpr int STEP = 2;                                // pixel step (trade-off speed vs precision)
				const int extent          = onEdge ? 12 : (nearEdge ? 8 : 6); // wider search near border (more warping/jitter)
				for (int oy = -extent; oy <= extent; oy += STEP) {            // oy = y offset (px)
					for (int ox = -extent; ox <= extent; ox += STEP) {        // ox = x offset (px)
						if (ox == 0 && oy == 0)
							continue;
						float rd   = 0.0f; // refined deltaL
						float rcsq = 0.0f; // refined chroma^2
						float rdf  = 0.0f; // refined dark support
						float rbf  = 0.0f; // refined bright support
						if (!computeMetricsAt(cx0 + ox, cy0 + oy, rd, rcsq, rdf, rbf))
							continue;
						if (rcsq > MAX_CHROMA_SQ)
							continue;
						static constexpr float MAX_CHROMA_SQ_WHITE        = 20.0f * 20.0f;            // normal white chroma cap
						static constexpr float MAX_CHROMA_SQ_WHITE_STRICT = 10.0f * 10.0f;            // stricter cap for weak-contrast whites
						const float edgeChromaCap                         = onEdge ? 150.0f : 200.0f; // extra clamp near border
						static constexpr float WEAK_WHITE_MARGIN          = 3.0f;                     // "weak" = near baseWhite
						if (rcsq > MAX_CHROMA_SQ_WHITE)
							continue;
						if (nearEdge && rcsq > edgeChromaCap)
							continue;
						if (rd < baseWhite + WEAK_WHITE_MARGIN && rcsq > MAX_CHROMA_SQ_WHITE_STRICT)
							continue;
						static constexpr float MIN_SUPPORT_FRAC = WHITE_MIN_SUPPORT_FRAC; // require strong bright support
						if (rbf < MIN_SUPPORT_FRAC)
							continue;
						if (rbf < rdf + 0.10f)
							continue;
						if (rd < baseWhite)
							continue;
						if (!found || rd > bestD) {
							found   = true;
							bestD   = rd;
							bestDf  = rdf;
							bestBf  = rbf;
							bestCsq = rcsq;
						}
					}
				}

				if (found) {
					d         = bestD;
					df        = bestDf;
					bf        = bestBf;
					csq       = bestCsq;
					candWhite = d >= baseWhite;
				}
			}
		}

		// 10.3 Reject non-candidates quickly.
		if (!candBlack && !candWhite)
			continue;

		// 10.4 Local contrast gating vs neighbor median.
		const float neigh = neighborMedianDelta(static_cast<int>(i)); // local deltaL baseline

		if (candBlack && d <= neigh - neighborMargin) {
			// Black stones should be dark and near-neutral; high chroma here tends to be wood/shadow artifacts.
			static constexpr float MAX_CHROMA_SQ_BLACK = 200.0f; // looser than white (black stones can pick up warm reflections)
			if (csq > MAX_CHROMA_SQ_BLACK)
				continue;
			static constexpr float MIN_SUPPORT_FRAC = 0.35f; // require dark support over a decent ROI fraction
			if (df < MIN_SUPPORT_FRAC)
				continue;
			if (df < bf + 0.10f)
				continue;
			stones.push_back(StoneState::Black);
			if (debugPrint)
				detectedIdx.push_back(i);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], innerRadius, cv::Scalar(0, 0, 0), 2);
			}
		} else if (candWhite && d >= neigh + neighborMargin) {
			// White stones are bright and near-neutral; apply chroma and support gating (stricter near edges).
			static constexpr float MAX_CHROMA_SQ_WHITE        = 20.0f * 20.0f;            // normal white chroma cap
			static constexpr float MAX_CHROMA_SQ_WHITE_STRICT = 10.0f * 10.0f;            // stricter cap for weak-contrast whites
			const float edgeChromaCap                         = onEdge ? 150.0f : 200.0f; // additional clamp near borders
			static constexpr float WEAK_WHITE_MARGIN          = 3.0f;                     // "weak" = near baseWhite
			if (csq > MAX_CHROMA_SQ_WHITE)
				continue;
			if (nearEdge && csq > edgeChromaCap)
				continue;
			if (d < baseWhite + WEAK_WHITE_MARGIN && csq > MAX_CHROMA_SQ_WHITE_STRICT)
				continue;
			// Near the board edge, empty intersections can look like low-contrast "white stones" (less grid coverage).
			// Be stricter for low-contrast whites near the border, but keep true edge stones (neutral chroma) working.
			{
				static constexpr float EDGE_WEAK_MARGIN        = 2.0f;  // extra contrast needed near edge
				static constexpr float EDGE_MAX_CHROMA_SQ_WEAK = 70.0f; // chroma cap for edge weak whites
				if (nearEdge && (d < baseWhite + EDGE_WEAK_MARGIN) && (csq > EDGE_MAX_CHROMA_SQ_WEAK))
					continue;
			}
			static constexpr float MIN_SUPPORT_FRAC = 0.40f; // require strong bright support
			if (bf < MIN_SUPPORT_FRAC)
				continue;
			if (bf < df + 0.10f)
				continue;
			stones.push_back(StoneState::White);
			if (debugPrint)
				detectedIdx.push_back(i);
			if (!vis.empty()) {
				cv::circle(vis, geometry.intersections[i], innerRadius, cv::Scalar(255, 0, 0), 2);
			}
		}
	}

#ifndef NDEBUG
	if (debugPrint) {
		// 11. Debug output (stderr/stdout): prints thresholds + detected indices.
		// NOTE: The per-index values printed below come from the precomputed arrays (pre-refinement).
		std::cout << "[StoneDebug] N=" << geometry.boardSize << " stones=" << stones.size() << " baseBlack=" << baseBlack << " baseWhite=" << baseWhite
		          << " neighborMargin=" << neighborMargin << " innerR=" << innerRadius << " bgR=" << bgRadius << " bgOffset=" << bgOffset << "\n";
		for (std::size_t idx: detectedIdx) {               // idx = flattened intersection index
			const int x   = static_cast<int>(idx) / N;     // grid x-index
			const int y   = static_cast<int>(idx) - x * N; // grid y-index
			const float d = deltaL[idx];                   // deltaL at idx (pre-refinement)
			std::cout << "  idx=" << idx << " (" << x << "," << y << ")"
			          << " d=" << d << " neigh=" << neighborMedianDelta(static_cast<int>(idx)) << " darkFrac=" << darkFrac[idx]
			          << " brightFrac=" << brightFrac[idx] << " chromaSq=" << chromaSq[idx] << "\n";
		}
		for (int tidx: debugTrackIdx) { // tidx = index requested by GO_CAMERA_STONE_DEBUG_TRACK
			if (tidx < 0 || static_cast<std::size_t>(tidx) >= deltaL.size())
				continue;
			const std::size_t idx = static_cast<std::size_t>(tidx);             // idx as size_t for indexing
			const int x           = static_cast<int>(idx) / N;                  // grid x-index
			const int y           = static_cast<int>(idx) - x * N;              // grid y-index
			const float d         = deltaL[idx];                                // deltaL at idx (pre-refinement)
			const float neigh     = neighborMedianDelta(static_cast<int>(idx)); // local deltaL baseline
			const bool candBlack  = d <= baseBlack;                             // coarse black candidate?
			const bool candWhite  = d >= baseWhite;                             // coarse white candidate?
			std::cout << "  [track] idx=" << idx << " (" << x << "," << y << ")"
			          << " d=" << d << " neigh=" << neigh << " candB=" << static_cast<int>(candBlack) << " candW=" << static_cast<int>(candWhite)
			          << " darkFrac=" << darkFrac[idx] << " brightFrac=" << brightFrac[idx] << " chromaSq=" << chromaSq[idx] << "\n";
		}
	}
#endif

	if (debugger) {
		// 12. Optional overlay visualization.
		debugger->add("Stone Overlay", vis);
		debugger->endStage();
	}

	// 13. Return detections.
	return {true, std::move(stones)};
}

} // namespace go::camera
