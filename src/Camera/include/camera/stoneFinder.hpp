#pragma once

#include "camera/debugVisualizer.hpp"
#include "camera/rectifier.hpp"

#include <opencv2/core/mat.hpp>
#include <vector>

namespace go::camera {

//! Stone state at a single grid intersection.
enum class StoneState { Empty, Black, White };

//! Result of the stone detection stage.
struct StoneResult {
	bool success;                   //!< True if detection ran successfully; false on invalid input.
	std::vector<StoneState> stones; //!< Detected stones (only non-empty entries are returned).
};

/**
 * @brief Detect stones on a rectified Go board image.
 *
 * @param geometry Rectified board geometry (image + intersections + spacing + boardSize).
 * @param debugger Optional debug visualizer for overlays (may be nullptr).
 * @return StoneResult where `stones` contains one entry per detected stone (Black/White).
 */
StoneResult analyseBoard(const BoardGeometry& geometry, go::camera::DebugVisualizer* debugger = nullptr);

} // namespace go::camera
