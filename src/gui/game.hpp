#pragma once

#include "board.hpp"
#include <SDL.h>

// TODO: Board in unique ptr.
class Game {
public:
    Game(int wndHeight, int wndWidth);
    ~Game();

    void run();

private:
    bool InitializeSDL();

private:
    bool m_exit = false;   //!< Stop game execution.
    bool m_redraw = true;  //!< Redraw window content.

    Board m_board;

    int m_windowWidth;
    int m_windowHeight;

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
};