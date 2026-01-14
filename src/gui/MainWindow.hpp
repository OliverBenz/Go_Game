#pragma once

#include "core/game.hpp"
#include <QMainWindow>

namespace go::gui {

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(Game& game, QWidget* parent = nullptr);
	~MainWindow() override;

private:
	//! Initial setup constructing the layout of the window.
	void buildLayout(Game& game);

private: // Slots
	void openConnectDialog();
	void closeEvent(QCloseEvent* event);

private:
	QWidget* m_menuWidget;
	QWidget* m_gameWidget;
};

} // namespace go::gui