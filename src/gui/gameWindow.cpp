#include "gameWindow.hpp"

#include <SDL2/SDL_image.h>
#include <iostream>

namespace go::sdl {

GameWindow::GameWindow(unsigned wndWidth, unsigned wndHeight, Game& game)
    : m_windowWidth{wndWidth}, m_windowHeight{wndHeight}, m_game{game} {
	if (initializeSDL()) {
		m_boardRenderer = std::make_unique<BoardRenderer>(game.board().size(), std::min(m_windowWidth, m_windowHeight),
		                                                  m_renderer);
	}

	m_game.addNotifiationListener(this);
}

GameWindow::~GameWindow() {
	m_game.removeNotificationListener(this);

	m_boardRenderer.reset();

	SDL_DestroyRenderer(m_renderer);
	SDL_DestroyWindow(m_window);

	SDL_Quit();
}

bool GameWindow::initializeSDL() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
		return false;
	}
	if (!(IMG_Init(IMG_INIT_WEBP | IMG_INIT_PNG) & (IMG_INIT_WEBP | IMG_INIT_PNG))) {
		std::cerr << "Failed to init webp module: " << IMG_GetError() << "\n";
		return false;
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

	m_window = SDL_CreateWindow("Go Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                            static_cast<int>(m_windowWidth), static_cast<int>(m_windowHeight), 0);
	if (!m_window) {
		std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
		return false;
	}

	m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
	if (!m_renderer) {
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

void GameWindow::sendRedrawEvent() {
	SDL_Event e;
	SDL_zero(e);

	e.type      = SDL_USEREVENT;
	e.user.code = EVENT_REDRAW;

	SDL_PushEvent(&e);
}

void GameWindow::run() {
	SDL_Event event;
	while (SDL_WaitEvent(&event) && !m_exit) {
		std::cerr << "Event: " << event.type << "\n";

		switch (event.type) {
		case SDL_QUIT:
			m_game.pushEvent(ShutdownEvent{});
			m_exit = true;
			break;

		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				Coord c;
				if (m_boardRenderer->pixelToCoord(event.button.x, event.button.y, c)) {
					m_game.pushEvent(PutStoneEvent{c});
				}
			}
			break;

		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			case SDLK_r:
				sendRedrawEvent();
				break;
			}
			break;

		case SDL_USEREVENT:
			if (event.user.code == EVENT_REDRAW) {
				SDL_RenderClear(m_renderer);
				m_boardRenderer->draw(m_game.board(), m_renderer);
				SDL_RenderPresent(m_renderer);
			}
			break;

		default:
			break;
		}
	}
}

void GameWindow::onBoardChange() {
	sendRedrawEvent();
}

} // namespace go::sdl
