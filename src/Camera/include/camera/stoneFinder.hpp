#pragma once

#include "camera/debugVisualizer.hpp"

#include <opencv2/core/mat.hpp>
#include <vector>

namespace go::camera {

enum class StoneState { Empty, Black, White };

struct StoneResult {
    bool success;
    std::vector<StoneState> board;
};

StoneResult analyseBoard(const cv::Mat& image, go::camera::DebugVisualizer* debugger);

}