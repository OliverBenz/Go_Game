#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);

	// Setup and show UI
	go::gui::MainWindow window;
	window.resize(1200, 900);
	window.show();

	const auto exitCode = application.exec();
	return exitCode;
}
