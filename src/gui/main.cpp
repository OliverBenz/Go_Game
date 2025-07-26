#include <cassert>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

constexpr int GAME_SIZE = 13;

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 800;

// Make space for GAME_SIZE many stones exactly
constexpr int BOARD_SIZE = static_cast<int>(std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / GAME_SIZE) * GAME_SIZE;
constexpr int STONE_SIZE = BOARD_SIZE / GAME_SIZE; // TODO: Could still be a pixel off due to rounding int division
                                                              //
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

// TODO: Move to own class and only construct board textures once.
// Draw function should not include texture loading, coordinate construction, size calc, etc
void drawBoard(SDL_Renderer* renderer) {
    static constexpr int LW = 2;
    static constexpr int DRAW_STEP_SIZE = STONE_SIZE/2;

    static constexpr int coordStart    = DRAW_STEP_SIZE;             //!< (x,y) starting coordinate of lines
    static constexpr int coordEnd      = BOARD_SIZE-DRAW_STEP_SIZE;  //!< (x,y) ending coordinate of lines
    static constexpr int effBoardWidth = coordEnd - coordStart;      //!< Board width between outer lines


    SDL_Rect dest_board{.x=0, .y=0, .w=BOARD_SIZE, .h=BOARD_SIZE};
    SDL_SetRenderDrawColor(renderer, 220, 179, 92, 255);
    SDL_RenderFillRect(renderer, &dest_board);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i != GAME_SIZE; ++i) {
        SDL_Rect dest_line {
            .x = coordStart,
            .y = coordStart + i * STONE_SIZE,
            .w = effBoardWidth,
            .h = LW
        };
        SDL_RenderFillRect(renderer, &dest_line);

        //SDL_RenderDrawLine(renderer, DRAW_STEP_SIZE, pos, BOARD_SIZE-DRAW_STEP_SIZE, pos);
    }
    for (int i = 0; i != GAME_SIZE; ++i) {
        SDL_Rect dest_line {
            .x = coordStart + i * STONE_SIZE,
            .y = coordStart,
            .w = LW,
            .h = effBoardWidth
        };
        SDL_RenderFillRect(renderer, &dest_line);

        // const int pos = i*STONE_SIZE - DRAW_STEP_SIZE;
        // SDL_RenderDrawLine(renderer, pos, DRAW_STEP_SIZE, pos, BOARD_SIZE-DRAW_STEP_SIZE);
    }
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

    SDL_Texture* black = load_texture("/home/oliver/Data/dev/Go_Game/assets/black.png", renderer);
    SDL_Texture* white = load_texture("/home/oliver/Data/dev/Go_Game/assets/white.png", renderer);
    if(!white || !black) {
        return -1;
    }

    // int board_width, board_height;
    // SDL_QueryTexture(board, nullptr, nullptr, &board_width, &board_height); 
    // assert(board_width == board_height);

    // int black_width, black_height;
    // SDL_QueryTexture(black, nullptr, nullptr, &black_width, &black_height); 

    // int white_width, white_height;
    // SDL_QueryTexture(white, nullptr, nullptr, &white_width, &white_height); 
    // assert(white_width == black_width);
    // assert(white_height == black_height);


    SDL_Rect dest_white;
    dest_white.w = STONE_SIZE;
    dest_white.h = STONE_SIZE;
    dest_white.x = (SCREEN_WIDTH - STONE_SIZE) / 2 + STONE_SIZE;
    dest_white.y = (SCREEN_HEIGHT - STONE_SIZE) / 2;

    SDL_Rect dest_black;
    dest_black.w = STONE_SIZE;
    dest_black.h = STONE_SIZE;
    dest_black.x = (SCREEN_WIDTH - STONE_SIZE) / 2;
    dest_black.y = (SCREEN_HEIGHT - STONE_SIZE) / 2;

    SDL_RenderClear(renderer);
    drawBoard(renderer);
    SDL_RenderCopy(renderer, black, nullptr, &dest_black);
    SDL_RenderCopy(renderer, white, nullptr, &dest_white);

    SDL_RenderPresent(renderer);

    char inp = ' ';
    do {
        SDL_RenderClear(renderer);
        drawBoard(renderer);
        SDL_RenderCopy(renderer, black, nullptr, &dest_black);
        SDL_RenderCopy(renderer, white, nullptr, &dest_white);

        SDL_RenderPresent(renderer);
        inp = ' ';
        std::cin >> inp;
    } while(inp != 'e');


    // SDL_DestroyTexture(board);
    SDL_DestroyTexture(black);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
