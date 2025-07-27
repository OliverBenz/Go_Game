#include <array>
#include <cassert>
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

constexpr int GAME_SIZE = 9;

constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 800;

// Make space for GAME_SIZE many stones exactly
constexpr int BOARD_SIZE = static_cast<int>(std::min(SCREEN_WIDTH, SCREEN_HEIGHT) / GAME_SIZE) * GAME_SIZE;
constexpr int STONE_SIZE = BOARD_SIZE / GAME_SIZE; // TODO: Could still be a pixel off due to rounding int division



bool end_application = false;
bool redraw = true;

SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer){
    SDL_Surface* surface = IMG_Load(path);
    if (!surface) {
        std::cerr << "Failed to load '" << path << "': " << IMG_GetError() << "\n";
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (!tex) {
        std::cerr << "Could not load texture: " << SDL_GetError() << "\n";
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



    // TODO: Move stone rendering to own function 
    // TODO: Dont redraw background every time stones are updated. Only when stones are beaten
    // TODO: Only load textures once on startup.
    SDL_Texture* black = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_black.png", renderer);
    SDL_Texture* white = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_white.png", renderer);
    if(!white || !black) {
        return;
    }

    std::array<char, GAME_SIZE*GAME_SIZE> board{};

    // Manually set some stones for render testing
    board[0] = -1;
    board[4*GAME_SIZE + 2] = 1;
    board[5*GAME_SIZE + 1] = -1;
    board[GAME_SIZE*GAME_SIZE-1] = 1;

    for (std::size_t i = 0; i != board.size(); ++i) {
        if(board[i] == 0) {
            continue;
        }

        int x = i % GAME_SIZE;
        int y = i / GAME_SIZE;

        SDL_Rect dest;
        dest.w = STONE_SIZE;
        dest.h = STONE_SIZE;
        dest.x = (coordStart-DRAW_STEP_SIZE) + x * STONE_SIZE;
        dest.y = (coordStart-DRAW_STEP_SIZE) + y * STONE_SIZE;
        
        SDL_RenderCopy(renderer, (board[i] == 1 ? black : white), nullptr, &dest);
    }

    SDL_DestroyTexture(black);
    SDL_DestroyTexture(white);
}


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

    // int board_width, board_height;
    // SDL_QueryTexture(board, nullptr, nullptr, &board_width, &board_height); 
    // assert(board_width == board_height);

    // int black_width, black_height;
    // SDL_QueryTexture(black, nullptr, nullptr, &black_width, &black_height); 

    // int white_width, white_height;
    // SDL_QueryTexture(white, nullptr, nullptr, &white_width, &white_height); 
    // assert(white_width == black_width);
    // assert(white_height == black_height);


    SDL_RenderClear(renderer);
    drawBoard(renderer);
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
            drawBoard(renderer);
            SDL_RenderPresent(renderer);
            redraw = false;
        }
    }

    // SDL_DestroyTexture(board);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}
