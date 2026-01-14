#include "MainWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
	QApplication application(argc, argv);

	// Run game in game thread
	go::Game game(19u);
	std::thread gameThread([&] { game.run(); });

	// Setup and show UI
	go::gui::MainWindow window(game);
	window.resize(1200, 900);
	window.show();

	const auto exitCode = application.exec();

	game.pushEvent(go::ShutdownEvent{});
	gameThread.join();

	return exitCode;
}
