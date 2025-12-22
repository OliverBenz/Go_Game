#pragma once

#include "boardRenderer.hpp"
#include "core/IGameListener.hpp"
#include "core/game.hpp"

#include <QWidget>
#include <memory>

namespace go::ui {

class BoardWidget : public QWidget, public IGameListener {
	Q_OBJECT

public:
	explicit BoardWidget(Game& game, QWidget* parent = nullptr);
	~BoardWidget() override;

	//! Called by the game thread. Ensure not blocking.
	void onGameEvent(GameSignal signal) override;

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

private:
	//! Async queue render event. Prevents blocking of calling thread. 
	void queueRender();
	void translateClick(const QPoint& pos);
	void renderBoard();

	unsigned boardPixelSize() const;
	QPoint boardOffset(unsigned boardSize) const;

private:
	Game& m_game;

	bool m_listenerRegistered = false;
	unsigned m_lastBoardSize  = 0;
	BoardRenderer m_boardRenderer;
};

} // namespace go::ui
