#pragma once

#include "BoardWidget.hpp"
#include "app/sessionManager.hpp"
#include "core/game.hpp"
#include <QCloseEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
	void appendChatMessages();

private: // Slots
	void onPassClicked();
	void onResignClicked();
	void onSendChat();

private:
	app::SessionManager& m_game;

	BoardWidget* m_boardWidget = nullptr;
	QTabWidget* m_sideTabs     = nullptr;

	QLabel* m_statusLabel     = nullptr; //!< Game status text (active, finished).
	QLabel* m_currPlayerLabel = nullptr; //!< Current player text.

	QPushButton* m_passButton   = nullptr;
	QPushButton* m_resignButton = nullptr;

	QListWidget* m_chatList = nullptr;
	QLineEdit* m_chatInput  = nullptr;
	QPushButton* m_chatSend = nullptr;
	std::size_t m_chatCount = 0;
};

} // namespace go::gui
