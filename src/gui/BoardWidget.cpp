#include "BoardWidget.hpp"

#include "core/gameEvent.hpp"

#include <QColor>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace go::ui {

BoardWidget::BoardWidget(Game& game, QWidget* parent)
    : QWidget(parent), m_game(game), m_boardRenderer(static_cast<unsigned>(game.board().size())) {
	setFocusPolicy(Qt::StrongFocus); // Required to get key events.
	setMouseTracking(false);

	if (!m_listenerRegistered) {
		m_game.subscribeEvents(this, GS_BoardChange);
		m_listenerRegistered = true;
	}
}

BoardWidget::~BoardWidget() {
	if (m_listenerRegistered) {
		m_game.unsubscribeEvents(this);
	}
}

void BoardWidget::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
}

void BoardWidget::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	queueRender();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		translateClick(event->pos());
		event->accept();
        return;
	}

	QWidget::mouseReleaseEvent(event);
}

void BoardWidget::keyReleaseEvent(QKeyEvent* event) {
	std::cout << "KEY: " << event->key() << "\n";
	switch (event->key()) {
	case Qt::Key_P:
		m_game.pushEvent(PassEvent{});
		event->accept();
		return;

	case Qt::Key_R:
		m_game.pushEvent(ResignEvent{});
		event->accept();
		return;
	
		default:
		QWidget::keyReleaseEvent(event);
		return; // Don't accept the event.
	}
}

void BoardWidget::onGameEvent(GameSignal signal) {
	switch (signal) {
	case GS_BoardChange:
		queueRender(); // Async queue.
		break;
	default:
		break;
	}
}

void BoardWidget::queueRender() {
	QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
}

void BoardWidget::translateClick(const QPoint& pos) {
	const auto size   = boardPixelSize();
	const auto offset = boardOffset(size);
	const auto local  = pos - offset;
	if (size == 0u) {
		return;
	}
	if (local.x() < 0 || local.y() < 0 || local.x() >= static_cast<int>(size) || local.y() >= static_cast<int>(size)) {
		return;
	}

	Coord coord{};
	m_boardRenderer.setBoardSizePx(size);
	if (m_boardRenderer.pixelToCoord(local.x(), local.y(), coord)) {
		m_game.pushEvent(PutStoneEvent{coord});
	}
}

void BoardWidget::paintEvent(QPaintEvent* event) {
	QWidget::paintEvent(event);
	renderBoard();
}

void BoardWidget::renderBoard() {
	const auto size = boardPixelSize();
	if (size == 0u) {
		return;
	}

	if (size != m_lastBoardSize) {
		m_boardRenderer.setBoardSizePx(size);
		m_lastBoardSize = size;
	}

	const auto offset = boardOffset(size);
	QPainter painter(this);
	painter.fillRect(rect(), QColor(20, 20, 20));
	painter.save();
	painter.translate(offset);
	m_boardRenderer.draw(painter, m_game.board());
	painter.restore();
}

unsigned BoardWidget::boardPixelSize() const {
	const auto side = std::min(width(), height());
	return static_cast<unsigned>(std::max(side, 0));
}

QPoint BoardWidget::boardOffset(unsigned boardSize) const {
	const int dx = (width() - static_cast<int>(boardSize)) / 2;
	const int dy = (height() - static_cast<int>(boardSize)) / 2;
	return {dx, dy};
}

} // namespace go::ui
