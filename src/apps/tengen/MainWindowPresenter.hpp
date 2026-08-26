#pragma once

#include "GamePresenter.hpp"
#include "MainWindow.hpp"
#include "tengen/IGameSession.hpp"

#include <QObject>
#include <memory>

namespace tengen {

class MainWindowPresenter : public QObject {
	Q_OBJECT

public:
	explicit MainWindowPresenter(gui::MainWindow& mainWindow);
	~MainWindowPresenter() override;

private slots:
	void onNewLocalGameRequested();
	void onConnectRequested(const QString& hostIp);
	void onHostRequested(const unsigned boardSize);
	void onShutdownRequested();

private:
	void startOpenPlay();

private:
	gui::MainWindow& m_mainWindow;                             //!< The main window.
	std::unique_ptr<app::IGameSession> m_gameSession{nullptr}; //!< The actual game.
	std::unique_ptr<GamePresenter> m_gamePresenter{nullptr};   //!< The 'drawer' of the game.
};

} // namespace tengen
