#pragma once

#include "board.hpp"

#include <memory>
#include <SDL.h>

class Game {
public:
    Game(unsigned wndWidth, unsigned wndHeight);
    ~Game();

    void run();

private:
    bool InitializeSDL();

private:
    bool m_exit = false;     //!< Stop game execution.
    bool m_redraw = true;    //!< Redraw window content.
    bool m_turnBlack = true; //!< Black players turn.

    std::unique_ptr<Board> m_board = nullptr;

    unsigned m_windowWidth;
    unsigned m_windowHeight;

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
};