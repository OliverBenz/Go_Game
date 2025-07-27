#pragma once

#include "board.hpp"

#include <memory>
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

    std::unique_ptr<Board> m_board = nullptr;

    int m_windowWidth;
    int m_windowHeight;

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
};