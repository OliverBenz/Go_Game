#include <cassert>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

constexpr int SCREEN_WIDTH = 900;
constexpr int SCREEN_HEIGHT = 900;

SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer){
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        std::cerr << "Failed to load " << path << ": " << IMG_GetError() << "\n";
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex) {
        std::cerr << "No texture loaded";
    }
    SDL_FreeSurface(surface);
    return tex;
}

int main() {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL.\n";
        return -1;
    }
    if(!(IMG_Init(IMG_INIT_WEBP | IMG_INIT_PNG) & (IMG_INIT_WEBP|IMG_INIT_PNG))) {
        std::cerr << "Failed to init webp module.\n";
    }

    SDL_Window* window = SDL_CreateWindow("Go Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window){
        std::cerr << "Failed to create window\n";
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer){
        return -1;
    }

    SDL_Texture* board = load_texture("/home/oliver/Data/dev/Go_Game/assets/Blank_Go_board.webp", renderer);
    SDL_Texture* black = load_texture("/home/oliver/Data/dev/Go_Game/assets/black.png", renderer);
    SDL_Texture* white = load_texture("/home/oliver/Data/dev/Go_Game/assets/white.png", renderer);
    if(!board || !black) {
        return -1;
    }


    int black_width, black_height;
    SDL_QueryTexture(black, nullptr, nullptr, &black_width, &black_height); 

    int white_width, white_height;
    SDL_QueryTexture(white, nullptr, nullptr, &white_width, &white_height); 
    assert(white_width == black_width);
    assert(white_height == black_height);

    SDL_Rect dest_white;
    dest_white.w = black_width;
    dest_white.h = black_height;
    dest_white.x = (SCREEN_WIDTH - black_width) / 2 + black_width;
    dest_white.y = (SCREEN_HEIGHT - black_height) / 2;

    SDL_Rect dest_black;
    dest_black.w = black_width;
    dest_black.h = black_height;
    dest_black.x = (SCREEN_WIDTH - black_width) / 2;
    dest_black.y = (SCREEN_HEIGHT - black_height) / 2;

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, board, nullptr, nullptr);
    SDL_RenderCopy(renderer, black, nullptr, &dest_black);
    SDL_RenderCopy(renderer, white, nullptr, &dest_white);
    SDL_RenderPresent(renderer);

    SDL_Delay(5000);

    SDL_DestroyTexture(board);
    SDL_DestroyTexture(black);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
