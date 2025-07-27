#include "board.hpp"

#include <cassert>

Board::Board(int nodes, int boardSizePx, SDL_Renderer* renderer) :
    boardSize{static_cast<int>(boardSizePx / nodes) * nodes}, 
    stoneSize{boardSize / nodes},  // TODO: Could still be a pixel off due to rounding int division
    nodes(nodes),
    board(nodes*nodes, 0)
{
    textureBlack = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_black.png", renderer);
    textureWhite = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_white.png", renderer);
    if(!textureWhite || !textureBlack) {
        return;
    }

    // TODO: Remove debugging stones.
    addStone('A', 1, true, renderer);
    addStone('B', 1, true, renderer);
    addStone('C', 1, true, renderer);
    addStone('D', 2, false, renderer);
}

Board::~Board() {
    SDL_DestroyTexture(textureBlack);
    SDL_DestroyTexture(textureWhite);
}

// TODO: Don't redraw background and past stones all the time.
// TODO: Always draw new stone individually and add function redrawBoard
void Board::draw(SDL_Renderer* renderer) {
    drawBackground(renderer);
    drawStones(renderer);
}

SDL_Texture* Board::load_texture(const char* path, SDL_Renderer* renderer){
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

void Board::drawBackground(SDL_Renderer* renderer) {
    static constexpr int LW = 2;                             //!< Line width for grid
    static constexpr Uint8 COL_BG_R = 220;
    static constexpr Uint8 COL_BG_G = 179;
    static constexpr Uint8 COL_BG_B = 92;

    static const int effBoardWidth = coordEnd - coordStart;  //!< Board width between outer lines


    SDL_Rect dest_board{.x=0, .y=0, .w=boardSize, .h=boardSize};
    SDL_SetRenderDrawColor(renderer, COL_BG_R, COL_BG_G, COL_BG_B, 255);  // TODO: Define color
    SDL_RenderFillRect(renderer, &dest_board);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i != board.size()/2; ++i) {
        SDL_Rect dest_line {
            .x = coordStart,
            .y = coordStart + i * stoneSize,
            .w = effBoardWidth,
            .h = LW
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
    for (int i = 0; i != nodes; ++i) {
        SDL_Rect dest_line {
            .x = coordStart + i * stoneSize,
            .y = coordStart,
            .w = LW,
            .h = effBoardWidth
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
}

void Board::drawStone(int x, int y, int player, SDL_Renderer* renderer) {
    SDL_Rect dest;
    dest.w = stoneSize;
    dest.h = stoneSize;
    dest.x = (coordStart-drawStepSize) + x * stoneSize;
    dest.y = (coordStart-drawStepSize) + y * stoneSize;
    
    SDL_RenderCopy(renderer, (player == 1 ? textureBlack : textureWhite), nullptr, &dest);
}

void Board::drawStones(SDL_Renderer* renderer) {
    for (int i = 0; i != board.size(); ++i) {
        if(board[i] == 0) {
            continue;
        }

        int x = i % nodes;
        int y = i / nodes;

        drawStone(x, y, board[i], renderer);
    }
}

// TODO: Use unsigned types.
int Board::coordToId(int x, int y) {
    assert(0 <= x <= nodes-1 && 0 <= y <= nodes-1);

    return y * nodes + x;
}

bool Board::addStone(char x, int y, bool black, SDL_Renderer* renderer) {
    const int xTrafo = static_cast<int>(std::tolower(x)) - 97;
    const int yTrafo = nodes-y;

    if(!(0 <= xTrafo <= nodes-1 && 0 <= yTrafo <= nodes-1)) {
        std::cerr << "Invalid coordinates for board size.\n";
        return false;
    }

    board[coordToId(xTrafo, yTrafo)] = (black ? 1 : -1);

    drawStone(xTrafo, yTrafo, black ? 1 : -1, renderer);

    return true;
}