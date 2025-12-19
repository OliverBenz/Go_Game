#pragma once

#include "boardRenderer.hpp"
#include "core/IGameListener.hpp"
#include "core/game.hpp"

#include <SDL.h>
#include <memory>

namespace go::sdl {

class GameWindow : public IGameListener {
public:
	GameWindow(unsigned wndWidth, unsigned wndHeight, Game& game);
	~GameWindow() override;

	void run();                    //!< Start SDL Event queue handling.
	void onBoardChange() override; //!< Game signals board change here.

private:
	bool initializeSDL();
	void sendRedrawEvent();

private:
	unsigned m_windowWidth;  //!< Window width in pixels.
	unsigned m_windowHeight; //!< Window height in pixels.
	bool m_exit  = false;    //!< Stop game execution.
	bool m_ready = false;    //!< SDL initialized and renderer available.

	Game& m_game;

	std::unique_ptr<BoardRenderer> m_boardRenderer = nullptr;
	SDL_Window* m_window                           = nullptr;
	SDL_Renderer* m_renderer                       = nullptr;

	static constexpr int EVENT_REDRAW = 2; //!< Event ID for a redraw.
};

} // namespace go::sdl
