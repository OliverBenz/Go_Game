#pragma once

#include "SdlBoardWidget.hpp"
#include "core/game.hpp"

#include <QCloseEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTabWidget>

namespace go::ui {

class MainWindow : public QMainWindow, public IGameListener {
	Q_OBJECT

public:
	explicit MainWindow(Game& game, QWidget* parent = nullptr);
	~MainWindow() override = default;

	void onGameEvent(GameSignal signal) override;

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	void buildLayout();

private:
	Game& m_game;

	SdlBoardWidget* m_boardWidget = nullptr;
	QLabel* m_statusLabel         = nullptr;
	QTabWidget* m_sideTabs        = nullptr;
	QPushButton* m_passButton     = nullptr;
	QPushButton* m_resignButton   = nullptr;
	QLabel* m_currPlayerLabel     = nullptr;
};

} // namespace go::ui
