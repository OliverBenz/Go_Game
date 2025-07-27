#include <array>
#include <cassert>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

#include "board.hpp"

constexpr int GAME_SIZE = 9;

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 800;

// Make space for GAME_SIZE many stones exactly
constexpr int BOARD_SIZE = static_cast<int>(std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / GAME_SIZE) * GAME_SIZE;
constexpr int STONE_SIZE = BOARD_SIZE / GAME_SIZE; // TODO: Could still be a pixel off due to rounding int division

bool end_application = false;
bool redraw = true;

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << "\n";
        return -1;
    }
    if(!(IMG_Init(IMG_INIT_WEBP | IMG_INIT_PNG) & (IMG_INIT_WEBP|IMG_INIT_PNG))) {
        std::cerr << "Failed to init webp module: " << IMG_GetError() << "\n";
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    SDL_Window* window = SDL_CreateWindow("Go Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window){
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer){
        std::cerr << "Could not create renderer: " << SDL_GetError() << "\n";
        return -1;
    }


    // These events are not useful for Go. Only want mouse klick and some keyboard presses.
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEWHEEL, SDL_IGNORE);

    SDL_EventState(SDL_FINGERMOTION, SDL_IGNORE);
    SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);

    // int black_width, black_height;
    // SDL_QueryTexture(black, nullptr, nullptr, &black_width, &black_height); 

    // int white_width, white_height;
    // SDL_QueryTexture(white, nullptr, nullptr, &white_width, &white_height); 
    // assert(white_width == black_width);
    // assert(white_height == black_height);

    Board board(9, std::min(SCREEN_WIDTH, SCREEN_HEIGHT), renderer);

    SDL_RenderClear(renderer);
    board.draw(renderer);
    SDL_RenderPresent(renderer);
    redraw = false;

    SDL_Event event;
    while(SDL_WaitEvent(&event) && !end_application) {
        std::cerr << "Event: " << event.type << "\n";

        switch(event.type) {
        case SDL_QUIT:
            end_application = true;
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int x = event.button.x;
                int y = event.button.y;
                std::cout << "Left click released at (" << x << ", " << y << ")\n";
            }
            break;

        case SDL_KEYUP:
            switch(event.key.keysym.sym) {
            case SDLK_r:
                  redraw = true;
                  break;
            }
            break;

        default:
            break;
        }

        if(redraw) {
            SDL_RenderClear(renderer);
            board.draw(renderer);
            SDL_RenderPresent(renderer);
            redraw = false;
        }
    }

    // TODO: Remove explicit call once we have game class.
    board.~Board();

    // SDL_DestroyTexture(board);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
