#include "game.hpp"

#include <iostream>
#include <SDL2/SDL_image.h>

Game::Game(unsigned wndWidth, unsigned wndHeight) : m_windowWidth{wndWidth}, m_windowHeight{wndHeight}
{
    if(InitializeSDL()) {
        m_board = std::make_unique<Board>(9u, std::min(m_windowWidth, m_windowHeight), m_renderer);
    }
}

Game::~Game() {
    m_board.reset();

    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);

    SDL_Quit();
}

bool Game::InitializeSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return false;
    }
    if(!(IMG_Init(IMG_INIT_WEBP | IMG_INIT_PNG) & (IMG_INIT_WEBP|IMG_INIT_PNG))) {
        std::cerr << "Failed to init webp module: " << IMG_GetError() << "\n";
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    m_window = SDL_CreateWindow("Go Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(m_windowWidth), static_cast<int>(m_windowHeight), 0);
    if (!m_window){
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if(!m_renderer){
        std::cerr << "Could not create renderer: " << SDL_GetError() << "\n";
        return false;
    }

    // These events are not useful for Go. Only want mouse klick and some keyboard presses.
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEWHEEL, SDL_IGNORE);

    SDL_EventState(SDL_FINGERMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);

    return true;
}

void Game::run() {
    SDL_Event event;
    while(SDL_WaitEvent(&event) && !m_exit) {
        std::cerr << "Event: " << event.type << "\n";

        switch(event.type) {
        case SDL_QUIT:
            m_exit = true;
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const auto x = event.button.x;
                const auto y = event.button.y;
                if(m_board->addStone(x, y, m_turnBlack, m_renderer)) {
                    m_turnBlack = !m_turnBlack;
                    m_redraw = true;
                }
            }
            break;

        case SDL_KEYUP:
            switch(event.key.keysym.sym) {
            case SDLK_r:
                m_redraw = true;
                break;
            }
            break;

        default:
            break;
        }

        if(m_redraw) {
            SDL_RenderClear(m_renderer);
            m_board->draw(m_renderer);
            SDL_RenderPresent(m_renderer);
            m_redraw = false;
        }
    }
}