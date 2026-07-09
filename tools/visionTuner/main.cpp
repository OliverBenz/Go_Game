#include "analyser.hpp"
#include "mainWindow.hpp"
#include "pipelineStep.hpp"
#include "webcam.hpp"

#include <QApplication>
#include <opencv2/opencv.hpp>

#include <filesystem>
#include <iostream>


namespace tengen {

static std::filesystem::path resolveInputPath(const int argc, char** argv) {
	if (argc > 1) {
		return std::filesystem::path(argv[1]);
	}
	return std::filesystem::path(PATH_TEST_IMG) / "setup/C2_1.png";
}

static cv::Mat loadFallbackImage(const int argc, char** argv) {
	const auto inputPath = resolveInputPath(argc, argv);
	cv::Mat image        = cv::imread(inputPath.string(), cv::IMREAD_COLOR);
	if (image.empty()) {
		std::cerr << "Failed to load fallback image: " << inputPath << "\n";
	}
	return image;
}

int run(int argc, char** argv) {
	QApplication application(argc, argv);

	MainWindow window;
	window.resize(1400, 900);
	cv::Mat fallbackImage;

	PipelineStep currentStep{PipelineStep::FindBoard};

	Webcam webcam(0, [&](const cv::Mat& frame) { window.setImage(vision::analyse(frame, currentStep)); });
	webcam.setCaptureRate(1000); // TODO: Use chrono ms

	QObject::connect(&window, &MainWindow::videoCaptureClicked, [&] { webcam.capture(); });
	QObject::connect(&window, &MainWindow::imageSourceChanged, [&](const ImageSource source) {
		switch (source) {
		case ImageSource::Photo:
			if (webcam.isRunning()) {
				webcam.stopLive();
			}
			webcam.capture();
			break;
		case ImageSource::Video:
			if (!webcam.isRunning()) {
				webcam.startLive();
			}
			break;
		default:
			break;
		}
	});
	QObject::connect(&window, &MainWindow::pipelineStepChanged, [&](const PipelineStep step) { currentStep = step; });

	window.show();

	const int exitCode = application.exec();
	webcam.stopLive();
	return exitCode;
}

} // namespace tengen

int main(int argc, char** argv) {
	return tengen::run(argc, argv);
}
