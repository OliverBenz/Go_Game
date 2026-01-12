#include "GameWindow.hpp"

#include "ConnectDialog.hpp"

#include <QHBoxLayout>
#include <QMenuBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <format>

namespace go::ui {

GameWindow::GameWindow(Game& game, QWidget* parent) : QMainWindow(parent), m_game(game) {
	// Setup Window
	setWindowTitle("Go Game");
	buildLayout();

	// Connect slots
	connect(m_passButton, &QPushButton::clicked, this, &GameWindow::onPassClicked);
	connect(m_resignButton, &QPushButton::clicked, this, &GameWindow::onResignClicked);

	// Setup Game Stuff
	setCurrentPlayerText();
	setGameStateText();
	m_game.subscribeSignals(this, GS_PlayerChange | GS_StateChange);
}

GameWindow::~GameWindow() {
	m_game.unsubscribeSignals(this);
}

void GameWindow::onGameEvent(const GameSignal signal) {
	// TODO: This is called by core game thread
	//  Game thread should not update stuff on the UI but push notifications and let the UI handle it.
	switch (signal) {
	case GS_PlayerChange:
		setCurrentPlayerText();
		break;
	case GS_StateChange:
		setGameStateText();
		break;
	default:
		break;
	}
}

void GameWindow::closeEvent(QCloseEvent* event) {
	m_game.pushEvent(ShutdownEvent{});
	QMainWindow::closeEvent(event);
}

void GameWindow::buildLayout() {
	// Menu Bar
	auto* menu          = menuBar()->addMenu(tr("&Menu"));
	auto* connectAction = new QAction("&Connect to Server", this);
	menu->addAction(connectAction);
	connect(connectAction, &QAction::triggered, this, &GameWindow::openConnectDialog);

	// Layout
	auto* central = new QWidget(this);

	// Layout Window top to bottom
	auto* mainLayout = new QVBoxLayout(central);
	mainLayout->setContentsMargins(12, 12, 12, 12);
	mainLayout->setSpacing(8);

	// Top: Label
	m_statusLabel = new QLabel("", central);
	mainLayout->addWidget(m_statusLabel);

	// Middle: Content section
	auto* contentLayout = new QHBoxLayout();
	contentLayout->setSpacing(12);

	m_boardWidget = new BoardWidget(m_game, central);
	m_boardWidget->setMinimumSize(640, 640);
	contentLayout->addWidget(m_boardWidget);

	// Tabs on right side
	m_sideTabs = new QTabWidget(central);

	// Move History
	auto* moveHistoryTab = new QWidget(m_sideTabs);
	auto* moveLayout     = new QVBoxLayout(moveHistoryTab); // Title and content below
	moveLayout->addWidget(new QLabel("Move history will be listed here.", moveHistoryTab));
	moveLayout->addStretch(); // Fill space below Label
	m_sideTabs->addTab(moveHistoryTab, "Moves");

	// Chat
	auto* chatTab    = new QWidget(m_sideTabs);
	auto* chatLayout = new QVBoxLayout(chatTab); // Title and content below
	chatLayout->addWidget(new QLabel("Chat placeholder.", chatTab));
	chatLayout->addStretch(); // Fill space below Label
	m_sideTabs->addTab(chatTab, "Chat");

	contentLayout->addWidget(m_sideTabs, 1);
	mainLayout->addLayout(contentLayout, 1);

	// Bottom: Footer
	auto* footer       = new QWidget(central);
	auto* footerLayout = new QHBoxLayout(footer);
	footerLayout->setContentsMargins(0, 0, 0, 0);

	// Buttons
	m_passButton   = new QPushButton("Pass", footer);
	m_resignButton = new QPushButton("Resign", footer);
	footerLayout->addWidget(m_passButton);
	footerLayout->addWidget(m_resignButton);

	// Label
	m_currPlayerLabel = new QLabel("", footer);
	footerLayout->addWidget(m_currPlayerLabel);

	footer->setLayout(footerLayout);
	mainLayout->addWidget(footer);

	central->setLayout(mainLayout);
	setCentralWidget(central);
}

void GameWindow::setCurrentPlayerText() {
	const auto text = std::format("Current Player: {}", m_game.currentPlayer() == Player::Black ? "Black" : "White");
	m_currPlayerLabel->setText(QString::fromStdString(text));
}

void GameWindow::setGameStateText() {
	const auto text = std::format("Game: {}", m_game.isActive() ? "Active" : "Finished");
	m_statusLabel->setText(QString::fromStdString(text));
}

void GameWindow::onPassClicked() {
	m_game.pushEvent(PassEvent{m_game.currentPlayer()});
}

void GameWindow::onResignClicked() {
	m_game.pushEvent(ResignEvent{});
}

void GameWindow::openConnectDialog() {
	ConnectDialog dialog(this);

	if (dialog.exec() == QDialog::Accepted) {
		const auto ip = dialog.ipAddress().toStdString();
	}
}

} // namespace go::ui
