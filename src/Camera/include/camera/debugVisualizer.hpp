#pragma once

#include <opencv2/core/mat.hpp>

#include <string>
#include <vector>

namespace go::camera {

//! Each step in the algorithm.
struct DebugStep {
	std::string name; //!< Some name.
	cv::Mat image;    //!< Image produced by the step.
};

//! Our Go detection algorithm has multiple stages. We collect the images per stage.
struct DebugStage {
	std::string name;                //!< Name of the stage.
	std::vector<DebugStep> images{}; //!< Image name pair for every step that was added.
};

//! Can be passed to our algorithm functions to get intermediate images for debugging purposes.
class DebugVisualizer {
public:
	void beginStage(std::string name);              //!< New stage in the algorithm starts.
	void add(std::string name, const cv::Mat& img); //!< Add an image given some step name. Show image in interactive mode.
	void endStage();

	cv::Mat buildMosaic(); //!< Returns mosaic of all debug images. Ends currently active stage.

	void setInteractive(bool interactive, unsigned displayTimeMs = 0u); //!< Enable immediate image display in add().
	void clear();

private:
	static cv::Mat toBgr8U(const cv::Mat& in);
	static cv::Mat labelTile(const cv::Mat& tile, const std::string& text);

private:
	bool m_interactive{false};  //!< Immediately show image when it's added.
	unsigned m_displayTime{0u}; //!< How many ms to show the image in interactive mode. 0->inf.

	DebugStage m_currentStage{};        //!< Currently active stage.
	bool m_hasActiveStage{false};       //!< A stage is active.
	std::vector<DebugStage> m_stages{}; //!< Collection of debug info for all stages.
};

} // namespace go::camera
