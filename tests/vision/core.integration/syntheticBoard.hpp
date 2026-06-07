#pragma once

#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <opencv2/opencv.hpp>

namespace tengen::vision::core {
namespace gtest {

//! Draw a synthetic board image in canonical board coordinates.
cv::Mat makeCanonicalBoardImage(unsigned boardSize, int sidePx);

//! Warp a canonical board image into a scene using a destination quadrilateral.
cv::Mat warpBoardToScene(const cv::Mat& board, cv::Size sceneSize, const std::array<cv::Point2f, 4>& dstQuad);

//! Build a synthetic image with a clearly visible board outline but no internal grid.
cv::Mat makeOutlineOnlySyntheticScene();

//! Build a high-contrast synthetic image with a board that almost touches all image borders.
cv::Mat makeFullFrameSyntheticScene();

//! Create a synthetic, perfectly rectified board with evenly spaced intersections.
RectifiedBoard makeSyntheticBoard(unsigned N, double spacingPx, const cv::Scalar& woodBgr);

//! Draw a filled stone at a given grid coordinate (gx,gy).
void drawStone(RectifiedBoard& g, unsigned gx, unsigned gy, StoneState s);

} // namespace gtest
} // namespace tengen::vision::core
