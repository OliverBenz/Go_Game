#pragma once

#include "app/sessionManager.hpp"
#include "boardRenderer.hpp"
#include "core/game.hpp"
#include <QWidget>

namespace go::gui {

class BoardWidget : public QWidget, public app::IAppSignalListener {
	Q_OBJECT

public:
	explicit BoardWidget(app::SessionManager& game, QWidget* parent = nullptr);
	~BoardWidget() override;

	//! Called by the game thread. Ensure not blocking.
	void onAppEvent(app::AppSignal signal) override;

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;

private:
	//! Asynchronously queue a render event. Prevents blocking of calling thread.
	void queueRender();
	//! Resolve click position to board coordinate and push game event if valid.
	void handleClick(const QPoint& pos);
	void renderBoard();

	//! Get the board size in pixels.
	unsigned boardPixelSize() const;
	//! Offset to get to the center of the board for drawing.
	QPoint boardOffset(unsigned boardSize) const;

private:
	app::SessionManager& m_game;

	bool m_listenerRegistered = false;
	BoardRenderer m_boardRenderer;
};

} // namespace go::gui
