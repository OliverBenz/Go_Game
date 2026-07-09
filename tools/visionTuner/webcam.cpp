#include "webcam.hpp"

#include <QObject>

namespace tengen {

Webcam::Webcam(const int cameraIndex, std::function<void(const cv::Mat&)> onImageCapture)
    : m_cameraIndex(cameraIndex), m_timer(500, [&]() { capture(); }), m_onCapture(std::move(onImageCapture)) {
}

Webcam::~Webcam() {
	stopLive();
}

bool Webcam::startLive() {
	if (m_capture.isOpened()) {
		return true;
	}

	if (!m_capture.open(m_cameraIndex)) {
		return false;
	}

	m_timer.start();

	return true;
}

void Webcam::setCaptureRate(const unsigned periodMs) {
	m_timer.setPeriod(periodMs);
}

bool Webcam::isRunning() {
	return m_capture.isOpened() && m_timer.isActive();
}

void Webcam::stopLive() {
	m_timer.stop();
	if (m_capture.isOpened()) {
		m_capture.release();
	}
}

void Webcam::capture() {
	assert(m_onCapture); // Connect a callback. Otherwise we just do nothing but waste resources.

	if (!m_capture.isOpened() && !m_capture.open(m_cameraIndex)) {
		return;
	}

	// Drain any backlog. Buffered grabs return near-instantly;
	// a grab that has to wait for a genuinely new frame takes ~1 frame interval.
	using namespace std::chrono;
	for (;;) {
		auto t0 = steady_clock::now();
		if (!m_capture.grab())
			return;
		auto dt = duration_cast<milliseconds>(steady_clock::now() - t0).count();
		if (dt > 5)
			break; // had to wait -> this is a fresh frame
	}

	cv::Mat frame;
	if (!m_capture.retrieve(frame) || frame.empty())
		return;
	m_onCapture(frame);
}

} // namespace tengen
