#pragma once

#include "boardRenderer.hpp"
#include "core/IGameListener.hpp"
#include "core/game.hpp"

#include <QWidget>
#include <memory>

namespace go::ui {

class SdlBoardWidget : public QWidget, public IGameListener {
	Q_OBJECT

public:
	explicit SdlBoardWidget(Game& game, QWidget* parent = nullptr);
	~SdlBoardWidget() override;

	void onGameNotification(Notification event) override;

protected:
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event);

private:
	void ensureRenderer();
	void recreateBoardRenderer();
	void renderBoard();
	void queueRender();
	void translateClick(const QPoint& pos);

	unsigned boardPixelSize() const;
	QPoint boardOffset(unsigned boardSize) const;

private:
	Game& m_game;

	SDL_Window* m_sdlWindow   = nullptr;
	SDL_Renderer* m_renderer  = nullptr;
	bool m_initialized        = false;
	bool m_listenerRegistered = false;
	std::unique_ptr<go::sdl::BoardRenderer> m_boardRenderer;
};

} // namespace go::ui
