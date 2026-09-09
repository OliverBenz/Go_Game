#include "mainWindow.hpp"

#include <QApplication>

// Used for quickly visualising stuff I work on.
// Displays a test image (pipeline result overlaid + .json ground truth) next to the dotBW ground truth board.

// You select an image (usually in the tests/vision/resources. This app will load the image + .txt file + .json file and display what the pipeline does)
int main(int argc, char* argv[]) {
	QApplication application(argc, argv);

	tengen::MainWindow window;
	window.resize(1400, 800);
	window.show();

	return application.exec();
}
