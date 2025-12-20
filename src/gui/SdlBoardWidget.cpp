#include "SdlBoardWidget.hpp"

#include "core/gameEvent.hpp"

#include <QMetaObject>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <iostream>

namespace go::ui {

static bool ensureSdlInit() {
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
			return false;
		}
	}

	static bool imageReady = false;
	if (!imageReady) {
		if ((IMG_Init(IMG_INIT_WEBP | IMG_INIT_PNG) & (IMG_INIT_WEBP | IMG_INIT_PNG)) == 0) {
			std::cerr << "Failed to init SDL_image: " << IMG_GetError() << "\n";
			return false;
		}
		imageReady = true;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
	return true;
}

SdlBoardWidget::SdlBoardWidget(Game& game, QWidget* parent) : QWidget(parent), m_game(game) {
	setFocusPolicy(Qt::StrongFocus); // Required to get key events.

	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_OpaquePaintEvent);

	setMouseTracking(false);
}

SdlBoardWidget::~SdlBoardWidget() {
	if (m_listenerRegistered) {
		m_game.unsubscribeEvents(this);
	}

	m_boardRenderer.reset();
	if (m_renderer) {
		SDL_DestroyRenderer(m_renderer);
		m_renderer = nullptr;
	}
	if (m_sdlWindow) {
		SDL_DestroyWindow(m_sdlWindow);
		m_sdlWindow = nullptr;
	}
}

void SdlBoardWidget::showEvent(QShowEvent* event) {
	ensureRenderer();
	QWidget::showEvent(event);
}

void SdlBoardWidget::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	if (!m_initialized) {
		return;
	}
	recreateBoardRenderer();
	queueRender();
}

void SdlBoardWidget::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		translateClick(event->pos());
	}
	QWidget::mouseReleaseEvent(event);
}

void SdlBoardWidget::keyReleaseEvent(QKeyEvent* event) {
	std::cout << "KEY: " << event->key() << "\n";
	switch (event->key()) {
	case Qt::Key_P:
		m_game.pushEvent(PassEvent{});
		break;
	case Qt::Key_R:
		m_game.pushEvent(ResignEvent{});
		break;
	default:
		QWidget::keyPressEvent(event);
		return; // Don't accept the event.
	}

	event->accept(); // Set event handled
}

void SdlBoardWidget::onGameEvent(GameSignal signal) {
	switch(signal) {
	case GameSignal::BoardChange:
		queueRender();
		break;
	}
}

void SdlBoardWidget::ensureRenderer() {
	if (m_initialized) {
		return;
	}
	if (!ensureSdlInit()) {
		return;
	}

	// Get QT window Id and create SDL window there.
	m_sdlWindow = SDL_CreateWindowFrom(reinterpret_cast<void*>(static_cast<uintptr_t>(winId())));
	if (!m_sdlWindow) {
		std::cerr << "Failed to create SDL window from Qt surface: " << SDL_GetError() << "\n";
		return;
	}

	m_renderer = SDL_CreateRenderer(m_sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!m_renderer) {
		std::cerr << "Could not create renderer: " << SDL_GetError() << "\n";
		return;
	}

	recreateBoardRenderer();
	m_initialized = true;

	m_game.subscribeEvents(this, static_cast<uint64_t>(GameSignal::BoardChange));
	m_listenerRegistered = true;

	queueRender();
}

void SdlBoardWidget::recreateBoardRenderer() {
	assert(m_renderer);

	const auto boardSizePx = boardPixelSize();
	if (boardSizePx == 0u) {
		m_boardRenderer.reset();
		return;
	}
	m_boardRenderer = std::make_unique<go::sdl::BoardRenderer>(m_game.board().size(), boardSizePx, m_renderer);
}

void SdlBoardWidget::renderBoard() {
	assert(m_renderer && m_boardRenderer);
	if (!m_boardRenderer->isReady()) {
		return;
	}

	const auto size = boardPixelSize();
	if (size == 0u) {
		return;
	}

	SDL_SetRenderDrawColor(m_renderer, 20, 20, 20, 255);
	SDL_RenderClear(m_renderer);

	const auto offset = boardOffset(size);

	SDL_Rect viewport{offset.x(), offset.y(), static_cast<int>(size), static_cast<int>(size)};
	SDL_RenderSetViewport(m_renderer, &viewport);

	m_boardRenderer->draw(m_game.board(), m_renderer);

	SDL_RenderPresent(m_renderer);
	SDL_RenderSetViewport(m_renderer, nullptr);
}

void SdlBoardWidget::queueRender() {
	QMetaObject::invokeMethod(this, [this]() { renderBoard(); }, Qt::QueuedConnection);
}

void SdlBoardWidget::translateClick(const QPoint& pos) {
	if (!m_boardRenderer) {
		return;
	}

	const auto size   = boardPixelSize();
	const auto offset = boardOffset(size);
	const auto local  = pos - offset;
	if (size == 0u) {
		return;
	}
	if (local.x() < 0 || local.y() < 0 || local.x() > static_cast<int>(size) || local.y() > static_cast<int>(size)) {
		return;
	}

	Coord coord{};
	if (m_boardRenderer->pixelToCoord(local.x(), local.y(), coord)) {
		m_game.pushEvent(PutStoneEvent{coord});
	}
}

unsigned SdlBoardWidget::boardPixelSize() const {
	const auto side = std::min(width(), height());
	return static_cast<unsigned>(std::max(side, 0));
}

QPoint SdlBoardWidget::boardOffset(unsigned boardSize) const {
	const int dx = (width() - static_cast<int>(boardSize)) / 2;
	const int dy = (height() - static_cast<int>(boardSize)) / 2;
	return {dx, dy};
}

} // namespace go::ui
