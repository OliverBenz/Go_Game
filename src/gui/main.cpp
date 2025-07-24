#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

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
    if(!(IMG_Init(IMG_INIT_WEBP) & IMG_INIT_WEBP)) {
        std::cerr << "Failed to init webp module.\n";
    }

    SDL_Window* window = SDL_CreateWindow("Go Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 800, 0);
    if (!window){
        std::cerr << "Failed to create window\n";
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if(!renderer){
        return -1;
    }

    SDL_Texture* board = load_texture("/home/oliver/Data/dev/Go_Game/assets/Blank_Go_board.webp", renderer);
    if(!board) {
        return -1;
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, board, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    SDL_Delay(5000);

    SDL_DestroyTexture(board);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
