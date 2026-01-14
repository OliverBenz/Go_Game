#include "MainWindow.hpp"

#include "ConnectDialog.hpp"
#include "GameWidget.hpp"

#include <QMenuBar>

namespace go::gui {

MainWindow::MainWindow(Game& game, QWidget* parent) : QMainWindow(parent) {
	// Setup Window
	setWindowTitle("Go Game");
	buildLayout(game);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildLayout(Game& game) {
	// Menu Bar
	auto* menu          = menuBar()->addMenu(tr("&Menu"));
	auto* connectAction = new QAction("&Connect to Server", this);
	menu->addAction(connectAction);
	connect(connectAction, &QAction::triggered, this, &MainWindow::openConnectDialog);

	m_gameWidget = new GameWidget(game);
	setCentralWidget(m_gameWidget);
}

void MainWindow::openConnectDialog() {
	ConnectDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted) {
		const auto ip = dialog.ipAddress().toStdString();
	}
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QMainWindow::closeEvent(event);
}

} // namespace go::gui
