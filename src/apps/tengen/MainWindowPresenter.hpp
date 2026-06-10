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
	void onBotGameRequested(unsigned boardSize, unsigned difficultyIndex, bool humanPlaysBlack);
	void onConnectRequested(const QString& hostIp);
	void onHostRequested(const unsigned boardSize);
	void onShutdownRequested();

private:
	void startOpenPlay();

private:
	gui::MainWindow& m_mainWindow;
	std::unique_ptr<app::IGameSession> m_game{nullptr};
	std::unique_ptr<GamePresenter> m_gamePresenter{nullptr};
};

} // namespace tengen
