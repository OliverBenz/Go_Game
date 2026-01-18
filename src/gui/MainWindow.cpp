#include "MainWindow.hpp"

#include "ConnectDialog.hpp"
#include "GameWidget.hpp"
#include "HostDialog.hpp"

#include <QMenuBar>

namespace go::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
	// Setup Window
	setWindowTitle("Go Game");
	buildLayout();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildLayout() {
	// Menu Bar
	auto* menu          = menuBar()->addMenu(tr("&Menu"));
	auto* connectAction = new QAction("&Connect to Server", this);
	auto* hostAction    = new QAction("&Host Server", this);
	menu->addAction(connectAction);
	menu->addAction(hostAction);
	connect(connectAction, &QAction::triggered, this, &MainWindow::openConnectDialog);
	connect(hostAction, &QAction::triggered, this, &MainWindow::openHostDialog);

	m_gameWidget = new GameWidget(m_game);
	setCentralWidget(m_gameWidget);
}

void MainWindow::openConnectDialog() {
	ConnectDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted) {
		const auto ip = dialog.ipAddress().toStdString();
		m_game.connect(ip);
	}
}

void MainWindow::openHostDialog() {
	HostDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted) {
		m_game.host(dialog.boardSize());
	}
}

void MainWindow::closeEvent(QCloseEvent* event) {
	QMainWindow::closeEvent(event);
}

} // namespace go::gui
