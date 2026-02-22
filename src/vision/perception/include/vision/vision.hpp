#pragma once

#include "vision/core/rectifier.hpp" // TODO: Move to source. Dont propagate dependencies.
#include "vision/source.hpp"

#include <atomic>
#include <thread>

namespace tengen::vision {

struct StonePos {
	Coord c;
	float confidence;
};

/*! Manages the board and stone detection during the game.
 *  Process: The user
 *   - Calls Vision.setup(gaugeCoord) once. Homography and grid is stored and assumed static for the remaining game.
 *     The board has a natural D_4 symmetry. In the setup process, we have a single black stone placed on the board (not in the center!)
 *     to break the symmetry / to align the image rotation with the coordinate system.
 *   - Connects callback functions to be notified on different events.
 *   - Calls run() to start the board detection loop.
 */
class Vision {
public:
	struct Callbacks {
		std::function<void(StonePos)> onStonePlaced; //!< Callback when the image.
		std::function<void()> onBoardLost;           //!< Callback when the board or grid cannot be found anymore.
	};

public:
	explicit Vision(Source source = Source::Camera);

	//! Detect the board and grid once before the game starts.
	//! \param [in] gaugeStone Coordinate of a single black stone placed on the board. Used to fix the image orientation/coordinate mapping.
	//! \returns    True if the board and grid could be detected.
	bool setup(Coord gaugeCoord);

	void connect(Callbacks callback); //!< Connect callback functions.
	void disconnect();                //!< Disconnect the callback functions.

	//! Start the stone detection loop. Calls back to onStonePlaced when a new stone is detected.
	//! \note Only runs if source is a live feed.
	void run();

private:
	Source m_source;                //!< Source input for the board detection.
	core::BoardGeometry m_geometry; //!< Result of the setup process.

	Callbacks m_callbacks; //!< Callback functions to signal events.

	std::atomic<bool> m_running; //!< Stone detection loop running.
	std::thread m_visionThread;  //!< Thread handling the stone detection loop.
};

} // namespace tengen::vision
