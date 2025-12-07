#pragma once

#include "boardRenderer.hpp"

#include <memory>
#include <SDL.h>

class Window {
public:
    Window(unsigned wndWidth, unsigned wndHeight);
    ~Window();

    void run();

private:
    bool InitializeSDL();

private:
    bool m_exit = false;     //!< Stop game execution.
    bool m_redraw = true;    //!< Redraw window content.

    std::unique_ptr<BoardRenderer> m_board = nullptr;

    unsigned m_windowWidth;
    unsigned m_windowHeight;

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
};