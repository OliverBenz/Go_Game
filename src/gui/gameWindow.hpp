#pragma once

#include "core/game.hpp"
#include "boardRenderer.hpp"

#include <memory>
#include <SDL.h>

namespace go::sdl {

class GameWindow {
public:
    GameWindow(unsigned wndWidth, unsigned wndHeight, Game& game);
    ~GameWindow();

    void run();

private:
    bool InitializeSDL();

private:
    unsigned m_windowWidth;
    unsigned m_windowHeight;

    bool m_exit = false;     //!< Stop game execution.
    bool m_redraw = true;    //!< Redraw window content.

    Game& m_game;
    std::unique_ptr<BoardRenderer> m_board = nullptr;

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
};

}
