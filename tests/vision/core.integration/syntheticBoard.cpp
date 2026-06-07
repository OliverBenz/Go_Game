#include "syntheticBoard.hpp"

#include <array>
#include <gtest/gtest.h>

namespace tengen::vision::core {
namespace gtest {

RectifiedBoard makeSyntheticBoard(unsigned N, double spacingPx, const cv::Scalar& woodBgr) {
	RectifiedBoard g{};
	g.geometry.boardSize = N;
	g.geometry.spacing   = spacingPx;
	g.geometry.H         = cv::Mat::eye(3, 3, CV_64F);

	const int margin = static_cast<int>(std::lround(spacingPx)); //!< keep ROIs fully inside the image
	const int span   = static_cast<int>(std::lround(spacingPx * static_cast<double>(N - 1)));
	const int w      = 2 * margin + span;
	const int h      = 2 * margin + span;
	g.imageB         = cv::Mat(h, w, CV_8UC3, woodBgr);

	g.geometry.intersections.reserve(static_cast<std::size_t>(N) * static_cast<std::size_t>(N));
	for (unsigned gx = 0; gx < N; ++gx) {
		for (unsigned gy = 0; gy < N; ++gy) {
			const float x = static_cast<float>(margin + std::lround(static_cast<double>(gx) * spacingPx));
			const float y = static_cast<float>(margin + std::lround(static_cast<double>(gy) * spacingPx));
			g.geometry.intersections.emplace_back(x, y);
		}
	}

	return g;
}

cv::Mat warpBoardToScene(const cv::Mat& board, cv::Size sceneSize, const std::array<cv::Point2f, 4>& dstQuad) {
	cv::Mat scene(sceneSize, CV_8UC3, cv::Scalar(25, 25, 25));

	const std::array<cv::Point2f, 4> srcQuad = {
	        cv::Point2f(0.0f, 0.0f),
	        cv::Point2f(static_cast<float>(board.cols - 1), 0.0f),
	        cv::Point2f(static_cast<float>(board.cols - 1), static_cast<float>(board.rows - 1)),
	        cv::Point2f(0.0f, static_cast<float>(board.rows - 1)),
	};

	const cv::Mat H = cv::getPerspectiveTransform(srcQuad.data(), dstQuad.data());

	cv::Mat warpedBoard;
	cv::warpPerspective(board, warpedBoard, H, scene.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar::all(0));

	cv::Mat boardMask(board.rows, board.cols, CV_8UC1, cv::Scalar(255));
	cv::Mat warpedMask;
	cv::warpPerspective(boardMask, warpedMask, H, scene.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar::all(0));

	warpedBoard.copyTo(scene, warpedMask);

	return scene;
}

cv::Mat makeOutlineOnlySyntheticScene() {
	cv::Mat scene(900, 1300, CV_8UC3, cv::Scalar(20, 20, 20));
	const std::array<cv::Point, 4> quad = {cv::Point(12, 20), cv::Point(1284, 12), cv::Point(1288, 884), cv::Point(20, 892)};
	const std::vector<cv::Point> polygon(quad.begin(), quad.end());

	cv::fillConvexPoly(scene, polygon, cv::Scalar(205, 205, 205), cv::LINE_AA);
	cv::polylines(scene, polygon, true, cv::Scalar(35, 35, 35), 10, cv::LINE_AA);
	return scene;
}

cv::Mat makeFullFrameSyntheticScene() {
	const cv::Mat board                      = makeCanonicalBoardImage(13u, 900);
	const std::array<cv::Point2f, 4> dstQuad = {
	        cv::Point2f(8.0f, 12.0f),
	        cv::Point2f(1272.0f, 6.0f),
	        cv::Point2f(1278.0f, 948.0f),
	        cv::Point2f(14.0f, 944.0f),
	};
	return warpBoardToScene(board, cv::Size(1280, 960), dstQuad);
}

cv::Mat makeCanonicalBoardImage(unsigned boardSize, int sidePx) {
	cv::Mat board(sidePx, sidePx, CV_8UC3, cv::Scalar(145, 180, 208));

	const int margin     = static_cast<int>(std::lround(0.08 * static_cast<double>(sidePx)));
	const int gridSpan   = sidePx - 2 * margin;
	const double spacing = static_cast<double>(gridSpan) / static_cast<double>(boardSize - 1u);

	cv::rectangle(board, cv::Rect(margin, margin, gridSpan, gridSpan), cv::Scalar(45, 60, 70), 5, cv::LINE_AA);

	for (unsigned i = 0u; i < boardSize; ++i) {
		const int p = margin + static_cast<int>(std::lround(spacing * static_cast<double>(i)));
		cv::line(board, cv::Point(p, margin), cv::Point(p, margin + gridSpan), cv::Scalar(35, 45, 50), 2, cv::LINE_AA);
		cv::line(board, cv::Point(margin, p), cv::Point(margin + gridSpan, p), cv::Scalar(35, 45, 50), 2, cv::LINE_AA);
	}

	return board;
}

void drawStone(RectifiedBoard& g, unsigned gx, unsigned gy, StoneState s) {
	ASSERT_FALSE(g.imageB.empty());
	ASSERT_TRUE(g.geometry.boardSize == 9u || g.geometry.boardSize == 13u || g.geometry.boardSize == 19u);
	ASSERT_EQ(g.geometry.intersections.size(), g.geometry.boardSize * g.geometry.boardSize);

	const unsigned idx = gx * g.geometry.boardSize + gy;
	ASSERT_LT(idx, g.geometry.intersections.size());

	const int r          = static_cast<int>(std::lround(g.geometry.spacing * 0.40)); //!< < 0.5*spacing to avoid overlap
	const cv::Scalar col = (s == StoneState::Black) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);
	cv::circle(g.imageB, g.geometry.intersections[idx], r, col, cv::FILLED, cv::LINE_AA);
}

} // namespace gtest
} // namespace tengen::vision::core
