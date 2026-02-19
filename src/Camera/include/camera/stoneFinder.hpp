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
	std::vector<StoneState> stones; //!< Stone states aligned to BoardGeometry::intersections (size = boardSize * boardSize).
	std::vector<float> confidence;  //!< Per-intersection confidence for stones[i] (size = stones.size()). 0 -> Empty/unknown.
};

/*! Detect stones on a rectified Go board image.
 * \param [in]     geometry Rectified board geometry.
 * \param [in,out] debugger Optional debug visualizer for overlays.
 * \return         StoneResult where `stones[i]`/`confidence[i]` map to `geometry.intersections[i]`.
 */
StoneResult analyseBoard(const BoardGeometry& geometry, DebugVisualizer* debugger = nullptr);

} // namespace go::camera
