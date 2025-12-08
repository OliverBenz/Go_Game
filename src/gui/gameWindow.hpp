#pragma once

#include "core/IGameListener.hpp"
#include "core/game.hpp"
#include "boardRenderer.hpp"

#include <memory>
#include <SDL.h>

namespace go::sdl {

class GameWindow : public IGameListener {
public:
    GameWindow(unsigned wndWidth, unsigned wndHeight, Game& game);
    ~GameWindow() override;

    void run();

    void onBoardChange() override;

private:
    bool initializeSDL();
    void sendRedrawEvent();

private:
    unsigned m_windowWidth;  //!< Window width in pixels.
    unsigned m_windowHeight; //!< Window height in pixels.
    bool m_exit = false;     //!< Stop game execution.

    Game& m_game;

    std::unique_ptr<BoardRenderer> m_boardRenderer = nullptr;
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    static constexpr int EVENT_REDRAW = 2; //!< Event ID for a redraw. 
};

}
