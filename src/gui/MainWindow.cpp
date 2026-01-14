#include "MainWindow.hpp"

#include "ConnectDialog.hpp"

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
	menu->addAction(connectAction);
	connect(connectAction, &QAction::triggered, this, &MainWindow::openConnectDialog);

	// setCentralWidget(central);
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
