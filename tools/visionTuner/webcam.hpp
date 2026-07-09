#pragma once

#include "timer.hpp"

#include <functional>
#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>

namespace tengen {

class Webcam {
public:
	Webcam(int cameraIndex, std::function<void(const cv::Mat&)> onImageCapture);
	~Webcam();

	bool startLive();
	void stopLive();
	void setCaptureRate(unsigned periodMs);
	bool isRunning();

	void capture(); //!< Captures an image and calls the onCapture callback.

private:
	cv::VideoCapture m_capture{};
	int m_cameraIndex{0};

	Timer m_timer{};
	std::function<void(const cv::Mat& image)> m_onCapture{nullptr};
};

} // namespace tengen
