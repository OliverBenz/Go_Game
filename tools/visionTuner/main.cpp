#include "analyser.hpp"
#include "mainWindow.hpp"
#include "pipelineStep.hpp"
#include "webcam.hpp"

#include <QApplication>
#include <QFileDialog>
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

	PipelineStep currentStep{PipelineStep::FindBoard};
	cv::Mat currentImage = loadFallbackImage(argc, argv);

	// Re-run the selected pipeline step on whatever image is currently loaded (webcam frame or file).
	const auto refresh = [&] {
		if (!currentImage.empty()) {
			window.setImage(vision::analyse(currentImage, currentStep));
		}
	};

	Webcam webcam(0, [&](const cv::Mat& frame) {
		currentImage = frame;
		refresh();
	});
	webcam.setCaptureRate(1000); // TODO: Use chrono ms

	QObject::connect(&window, &MainWindow::videoCaptureClicked, [&] { webcam.capture(); });
	QObject::connect(&window, &MainWindow::loadImageClicked, [&] {
		const QString fileName = QFileDialog::getOpenFileName(&window, "Open Image", QString::fromStdString(std::filesystem::path(PATH_TEST_IMG).string()),
		                                                      "Images (*.png *.jpg *.jpeg *.bmp)");
		if (fileName.isEmpty()) {
			return;
		}
		if (webcam.isRunning()) {
			webcam.stopLive();
		}

		cv::Mat image = cv::imread(fileName.toStdString(), cv::IMREAD_COLOR);
		if (image.empty()) {
			std::cerr << "Failed to load image: " << fileName.toStdString() << "\n";
			return;
		}
		currentImage = image;
		refresh();
	});
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
	QObject::connect(&window, &MainWindow::pipelineStepChanged, [&](const PipelineStep step) {
		currentStep = step;
		refresh();
	});

	refresh(); // Show the fallback image right away.
	window.show();

	const int exitCode = application.exec();
	webcam.stopLive();
	return exitCode;
}

} // namespace tengen

int main(int argc, char** argv) {
	return tengen::run(argc, argv);
}
