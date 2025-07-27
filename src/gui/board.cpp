#include "board.hpp"

#include <cassert>
#include <iostream>
#include <SDL_image.h>

Board::Board(int nodes, int boardSizePx, SDL_Renderer* renderer) :
    m_boardSize{static_cast<int>(boardSizePx / nodes) * nodes}, 
    m_stoneSize{m_boardSize / nodes},  // TODO: Could still be a pixel off due to rounding int division
    m_nodes(nodes),
    m_board(nodes*nodes, 0)
{
    m_textureBlack = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_black.png", renderer);
    m_textureWhite = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_white.png", renderer);
    if(!m_textureWhite || !m_textureBlack) {
        return;
    }

    // TODO: Remove debugging stones.
    addStone('A', 1, true,  renderer);
    addStone('B', 1, true,  renderer);
    addStone('C', 1, true,  renderer);
    addStone('D', 2, false, renderer);
}

Board::~Board() {
    SDL_DestroyTexture(m_textureBlack);
    SDL_DestroyTexture(m_textureWhite);
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

    static const int effBoardWidth = m_coordEnd - m_coordStart;  //!< Board width between outer lines


    SDL_Rect dest_board{.x=0, .y=0, .w=m_boardSize, .h=m_boardSize};
    SDL_SetRenderDrawColor(renderer, COL_BG_R, COL_BG_G, COL_BG_B, 255);  // TODO: Define color
    SDL_RenderFillRect(renderer, &dest_board);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int i = 0; i != m_nodes; ++i) {
        SDL_Rect dest_line {
            .x = m_coordStart,
            .y = m_coordStart + i * m_stoneSize,
            .w = effBoardWidth,
            .h = LW
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
    for (int i = 0; i != m_nodes; ++i) {
        SDL_Rect dest_line {
            .x = m_coordStart + i * m_stoneSize,
            .y = m_coordStart,
            .w = LW,
            .h = effBoardWidth
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
}

void Board::drawStone(int x, int y, int player, SDL_Renderer* renderer) {
    SDL_Rect dest;
    dest.w = m_stoneSize;
    dest.h = m_stoneSize;
    dest.x = (m_coordStart - m_drawStepSize) + x * m_stoneSize;
    dest.y = (m_coordStart - m_drawStepSize) + y * m_stoneSize;
    
    SDL_RenderCopy(renderer, (player == 1 ? m_textureBlack : m_textureWhite), nullptr, &dest);
}

void Board::drawStones(SDL_Renderer* renderer) {
    for (int i = 0; i != m_board.size(); ++i) {
        if(m_board[i] == 0) {
            continue;
        }

        int x = i % m_nodes;
        int y = i / m_nodes;

        drawStone(x, y, m_board[i], renderer);
    }
}

// TODO: Use unsigned types.
int Board::coordToId(int x, int y) {
    assert(0 <= x <= m_nodes-1 && 0 <= y <= m_nodes-1);

    return y * m_nodes + x;
}

bool Board::addStone(char x, int y, bool black, SDL_Renderer* renderer) {
    const int xTrafo = static_cast<int>(std::tolower(x)) - 97; // ASCII dec('a')=79
    const int yTrafo = m_nodes-y;

    if(!(0 <= xTrafo <= m_nodes-1 && 0 <= yTrafo <= m_nodes-1)) {
        std::cerr << "Invalid coordinates for board size.\n";
        return false;
    }

    m_board[coordToId(xTrafo, yTrafo)] = (black ? 1 : -1);

    drawStone(xTrafo, yTrafo, black ? 1 : -1, renderer);

    return true;
}