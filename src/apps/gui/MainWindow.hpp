#pragma once

#include "tengen/sessionManager.hpp"

#include <QMainWindow>

namespace tengen::gui {

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

private:
	//! Initial setup constructing the layout of the window.
	void buildLayout();

private: // Slots
	void openConnectDialog();
	void openHostDialog();
	void closeEvent(QCloseEvent* event);

private:
	app::SessionManager m_game;

	QWidget* m_menuWidget;
	QWidget* m_gameWidget;
};

} // namespace tengen::gui
