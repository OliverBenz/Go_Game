#include "MainWindowPresenter.hpp"

#include "GamePresenter.hpp"
#include "tengen/networkSession.hpp"
#include "tengen/openSession.hpp"

#include <QObject>
#include <memory>

namespace tengen {

MainWindowPresenter::MainWindowPresenter(gui::MainWindow& mainWindow) : QObject(nullptr), m_mainWindow(mainWindow) {
	QObject::connect(&m_mainWindow, &gui::MainWindow::newLocalGameRequested, this, &MainWindowPresenter::onNewLocalGameRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::connectRequested, this, &MainWindowPresenter::onConnectRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::hostRequested, this, &MainWindowPresenter::onHostRequested);
	QObject::connect(&m_mainWindow, &gui::MainWindow::shutdownRequested, this, &MainWindowPresenter::onShutdownRequested);

	startOpenPlay();
}

MainWindowPresenter::~MainWindowPresenter() = default;

void MainWindowPresenter::startOpenPlay() {
	m_game          = std::make_unique<app::OpenSession>(9u);
	m_gamePresenter = std::make_unique<GamePresenter>(*m_game, m_mainWindow.gameWidget());
}

void MainWindowPresenter::onNewLocalGameRequested() {
	onShutdownRequested();
	startOpenPlay();
}

void MainWindowPresenter::onConnectRequested(const QString& hostIp) {
	onShutdownRequested();

	auto session = std::make_unique<app::NetworkSession>();
	session->connect(hostIp.toStdString());

	auto& game      = static_cast<app::IGameSession&>(*session);
	auto& chat      = static_cast<app::IChatSession&>(*session);
	m_gamePresenter = std::make_unique<GamePresenter>(game, m_mainWindow.gameWidget());
	m_gamePresenter->addChatWindow(chat);
	m_game = std::move(session);
}

void MainWindowPresenter::onHostRequested(const unsigned boardSize) {
	onShutdownRequested();

	auto session = std::make_unique<app::NetworkSession>();
	session->host(boardSize);

	auto& game      = static_cast<app::IGameSession&>(*session);
	auto& chat      = static_cast<app::IChatSession&>(*session);
	m_gamePresenter = std::make_unique<GamePresenter>(game, m_mainWindow.gameWidget());
	m_gamePresenter->addChatWindow(chat);
	m_game = std::move(session);
}

void MainWindowPresenter::onShutdownRequested() {
	if (m_game) {
		m_gamePresenter = nullptr; // Destroy before m_game
		m_game->shutdown();
		m_game = nullptr;
	}
}

} // namespace tengen
