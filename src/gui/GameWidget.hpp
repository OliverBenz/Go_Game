#pragma once

#include "BoardWidget.hpp"
#include "app/sessionManager.hpp"
#include "core/game.hpp"
#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QWidget>

namespace go::gui {

class GameWidget : public QWidget, public app::IAppSignalListener {
	Q_OBJECT

public:
	explicit GameWidget(app::SessionManager& game, QWidget* parent = nullptr);
	~GameWidget() override;

	//! Called by the game thread. Ensure not blocking.
	void onAppEvent(app::AppSignal signal) override;

private:
	//! Initial setup constructing the layout of the window.
	void buildNetworkLayout();

	void setCurrentPlayerText(); //!< Get current player from game and update the label.
	void setGameStateText();     //!< Get game state from game and update the label.

private: // Slots
	void onPassClicked();
	void onResignClicked();

private:
	app::SessionManager& m_game;

	BoardWidget* m_boardWidget = nullptr;
	QTabWidget* m_sideTabs     = nullptr;

	QLabel* m_statusLabel     = nullptr; //!< Game status text (active, finished).
	QLabel* m_currPlayerLabel = nullptr; //!< Current player text.

	QPushButton* m_passButton   = nullptr;
	QPushButton* m_resignButton = nullptr;
};

} // namespace go::gui
