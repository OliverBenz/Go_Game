#pragma once

#include <QCloseEvent>
#include <QMainWindow>
#include <QString>

namespace tengen::gui {

class GameWidget;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow() override;

	GameWidget& gameWidget();

signals:
	void newLocalGameRequested();
	void botGameRequested(unsigned boardSize, unsigned difficultyIndex, bool humanPlaysBlack);
	void connectRequested(const QString& hostIp);
	void hostRequested(unsigned boardSize);
	void shutdownRequested();

private:
	//! Initial setup constructing the layout of the window.
	void buildLayout();

private:
	void openBotDialog();
	void openConnectDialog();
	void openHostDialog();
	void openRulesDialog();

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	GameWidget* m_gameWidget = nullptr;
};

} // namespace tengen::gui
