#pragma once

#include "boardFinder.hpp"
#include "camera/debugVisualizer.hpp"

#include <opencv2/opencv.hpp>

// Rectifying is the first step in the board detection.
// Motivation: The image of a board is usually messy (contains background and the board in some angle). This makes it hard to analyse the board. This should be
// fixed before further processing. Goal:       Given a messy image of a board, produce an image where the board is visible from a top-down view without
// background and perfectly square edges. Process:
//   1) We detect a rough outline of the board (function warpToBoard) and warp to top-down view. Here, it's not yet clear if the border of the produced image is
//   correct. 2) We fine-tune this board detection (function rectifyImage). Using the projected board image, we detect grid lines, crop the image to the edge
//   grid lines + padding of a half stone.
namespace go::camera {

//! Final
struct BoardGeometry {
	cv::Mat image;                          //!< Image mapped to Board with padding.
	cv::Mat H;                              //!< Homography between the original image and the fine-tuned warped image.
	std::vector<cv::Point2f> intersections; //!< List of grid intersections on the board (in refined warped coordinates).
	double spacing;                         //!< Spacing between grid lines (in refined warped coordinated).
	unsigned boardSize;                     //!< Size of the go board (9, 13, 19).
};

//! Call this to produce the fully rectified image.
//! \param [in] image Warped image of a Go board (Board already detected).
//! \returns    Image showing Go board in a top-down view with the background cut out and a slight padding around the outermost edges(to not cut off stones at
//! the edges).
BoardGeometry rectifyImage(const cv::Mat& originalImg, const WarpResult& input, DebugVisualizer* debugger = nullptr);

} // namespace go::camera