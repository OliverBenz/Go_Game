#pragma once

#include "camera/debugVisualizer.hpp"

#include <opencv2/core/mat.hpp>

namespace tengen::vision::core {

struct WarpResult {
	cv::Mat image; //!< Image warped to fit the rough board contour.
	cv::Mat H;     //!< Homography used to apply the rough warping.
};

//! Detect rough Go board outline in an image and warp to center the board. Cut out background
//! \param [in] image Original unwarped image of a Go board.
//! \note       In the resulting warped image, it is not defined what exactly the border is. This is done in the second step (rectifyImage).
WarpResult warpToBoard(const cv::Mat& image, DebugVisualizer* debugger = nullptr);


} // namespace tengen::vision::core
