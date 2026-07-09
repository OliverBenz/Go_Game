#pragma once

#include "pipelineStep.hpp"

#include <opencv2/core/mat.hpp>

namespace tengen::vision {

cv::Mat analyse(const cv::Mat& image, PipelineStep step);

} // namespace tengen::vision
