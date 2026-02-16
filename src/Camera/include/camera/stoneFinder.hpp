#pragma once

#include "camera/debugVisualizer.hpp"
#include "camera/rectifier.hpp"

#include <opencv2/core/mat.hpp>
#include <vector>

namespace go::camera {

enum class StoneState { Empty, Black, White };

struct StoneResult {
    bool success;
    std::vector<StoneState> board;
};

StoneResult analyseBoard(const BoardGeometry& geometry, go::camera::DebugVisualizer* debugger);

}