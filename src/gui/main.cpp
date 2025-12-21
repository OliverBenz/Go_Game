#include "MainWindow.hpp"
#include "core/game.hpp"

#include <QApplication>
#include <thread>

int main(int argc, char* argv[]) {
	static constexpr std::size_t boardSize = 9u;

	QApplication application(argc, argv);

	// Run game in game thread
	go::Game game(boardSize);
	std::thread gameThread([&] { game.run(); });

	// Setup and show UI
	go::ui::MainWindow window(game);
	window.resize(1200, 900);
	window.show();

	const auto exitCode = application.exec();

	game.pushEvent(go::ShutdownEvent{});
	gameThread.join();
	return exitCode;
}
