#include "BoardWidget.hpp"

#include "Logging.hpp"
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

	m_boardRenderer.setBoardSizePx(boardPixelSize());

	queueRender();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent* event) {
#ifndef NDEBUG
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, std::format("Mouse click at: ({}, {})", event->pos().x(), event->pos().y()));
#endif

	if (event->button() == Qt::LeftButton) {
		handleClick(event->pos());
		event->accept();
		return;
	}

	QWidget::mouseReleaseEvent(event);
}

void BoardWidget::keyReleaseEvent(QKeyEvent* event) {
#ifndef NDEBUG
	auto logger = Logger();
	logger.Log(Logging::LogLevel::Info, std::format("Keyboard click: {}", event->key()));
#endif

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

void BoardWidget::handleClick(const QPoint& pos) {
	const auto sizePx = boardPixelSize();
	const auto local  = pos - boardOffset(sizePx);
	if (sizePx == 0u) {
		return;
	}

	// Clicked in bounds
	if (local.x() < 0 || local.y() < 0 || local.x() >= static_cast<int>(sizePx) || local.y() >= static_cast<int>(sizePx)) {
		return;
	}

	// Try push event
	Coord coord{};
	if (m_boardRenderer.pixelToCoord(local.x(), local.y(), coord)) {
		// TODO: Not use value from game.
		m_game.pushEvent(PutStoneEvent{m_game.currentPlayer(), coord});
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

	const auto offset = boardOffset(size);
	QPainter painter(this);
	painter.fillRect(rect(), QColor(20, 20, 20));
	painter.save();
	painter.translate(offset); // Center in drawing area
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
