#pragma once

#include "vision/core//debugVisualizer.hpp"
#include "vision/core/boardFinder.hpp"

#include <opencv2/geometry.hpp>
#include <opencv2/opencv.hpp>

// Rectifying is the first step in the board detection.
// Motivation: The image of a board is usually messy (contains background and the board in some angle). This makes it hard to analyse the board. This should be
// fixed before further processing. Goal:       Given a messy image of a board, produce an image where the board is visible from a top-down view without
// background and perfectly square edges. Process:
//   1) We detect a rough outline of the board (function warpToBoard) and warp to top-down view. Here, it's not yet clear if the border of the produced image is correct.
//   2) We analyse the board geometry (function analyseGeometry). Using the projected board image, we detect grid lines, crop the image to the edge
//      grid lines + padding of a half stone and compute the homography for the final image transform.
namespace tengen::vision::core {

struct BoardGeometry {
	cv::Mat H;                              //!< Homography H between the original image and the fine-tuned warped image.
	std::vector<cv::Point2f> intersections; //!< List of grid intersections on the board (in refined warped coordinates).
	double spacing;                         //!< Spacing between grid lines (in refined warped coordinated).
	unsigned boardSize;                     //!< Size of the go board (9, 13, 19).
};

//! Final output of the rectification stage.
struct RectifiedBoard {
	cv::Mat imageB;         //!< Image in B space (mapped to board with padding).
	BoardGeometry geometry; //!< Geometry aligned with imageB.
};

//! Analyse the projected board image and compute the refined board geometry.
//! \param [in] input Warped image of a Go board (board already detected).
//! \returns    Board geometry in refined board coordinates.
BoardGeometry analyseGeometry(const WarpResult& input, DebugVisualizer* debugger = nullptr);

//! Transform the original image into the refined board image space described by geometry.H.
//! \returns    Rectified board image together with the geometry it matches.
RectifiedBoard transformImage(const cv::Mat& originalImg, const BoardGeometry& geometry, DebugVisualizer* debugger = nullptr);

bool isValidGeometry(const BoardGeometry& geometry);
bool isValidRectifiedBoard(const RectifiedBoard& board);

} // namespace tengen::vision::core
