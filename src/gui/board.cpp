#include "board.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <format>
#include <SDL_image.h>

Board::Board(unsigned nodes, unsigned boardSizePx, SDL_Renderer* renderer) :
    m_boardSize{boardSizePx / nodes * nodes}, 
    m_stoneSize{m_boardSize / nodes},  // TODO: Could still be a pixel off due to rounding int division
    m_nodes(nodes),
    m_board(nodes*nodes, 0)
{
    m_textureBlack = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_black.png", renderer);
    m_textureWhite = load_texture("/home/oliver/Data/dev/Go_Game/assets/anime_white.png", renderer);
    if(!m_textureWhite || !m_textureBlack) {
        return;
    }
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
    static constexpr int LW = 2;            //!< Line width for grid
    static constexpr Uint8 COL_BG_R = 220;
    static constexpr Uint8 COL_BG_G = 179;
    static constexpr Uint8 COL_BG_B = 92;

    assert(m_coordEnd > m_coordStart);
    static const int effBoardWidth = static_cast<int>(m_coordEnd - m_coordStart);  //!< Board width between outer lines


    SDL_Rect dest_board{.x=0, .y=0, .w=static_cast<int>(m_boardSize), .h=static_cast<int>(m_boardSize)};
    SDL_SetRenderDrawColor(renderer, COL_BG_R, COL_BG_G, COL_BG_B, 255);
    SDL_RenderFillRect(renderer, &dest_board);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (unsigned i = 0; i != m_nodes; ++i) {
        SDL_Rect dest_line {
            .x = static_cast<int>(m_coordStart),
            .y = static_cast<int>(m_coordStart + i * m_stoneSize),
            .w = effBoardWidth,
            .h = LW
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
    for (unsigned i = 0; i != m_nodes; ++i) {
        SDL_Rect dest_line {
            .x = static_cast<int>(m_coordStart + i * m_stoneSize),
            .y = static_cast<int>(m_coordStart),
            .w = LW,
            .h = effBoardWidth
        };
        SDL_RenderFillRect(renderer, &dest_line);
    }
}

void Board::drawStone(unsigned x, unsigned y, int player, SDL_Renderer* renderer) {
    assert(m_coordStart >= m_drawStepSize);

    SDL_Rect dest;
    dest.w = static_cast<int>(m_stoneSize);
    dest.h = static_cast<int>(m_stoneSize);
    dest.x = static_cast<int>((m_coordStart - m_drawStepSize) + x * m_stoneSize);
    dest.y = static_cast<int>((m_coordStart - m_drawStepSize) + y * m_stoneSize);
    
    SDL_RenderCopy(renderer, (player == 1 ? m_textureBlack : m_textureWhite), nullptr, &dest);
}

void Board::drawStones(SDL_Renderer* renderer) {
    for (unsigned i = 0; i != m_board.size(); ++i) {
        if(m_board[i] == 0) {
            continue;
        }

        unsigned x = i % m_nodes;
        unsigned y = i / m_nodes;

        drawStone(x, y, m_board[i], renderer);
    }
}

unsigned Board::coordToId(unsigned x, unsigned y) {
    assert(x < m_nodes && y < m_nodes);

    return y * m_nodes + x;
}

bool Board::pixelToCoord(int px, unsigned& coord) {
    static constexpr float TOLERANCE = 0.3f;  // To avoid accidental placement of stones.

    const auto coordRel  = static_cast<float>(px-static_cast<int>(m_coordStart)) / static_cast<float>(m_stoneSize);  // Calculate board coordinate from pixel values.
    const auto coordRound = std::round(coordRel);                               // Round to nearest coordinate.

    // Click has to be close enough to a point and on the board.
    if(std::abs(coordRound - coordRel) > TOLERANCE || coordRound < 0 || coordRound > static_cast<float>(m_nodes)-1) {
        std::cerr << "Could not assign mouse coordinats to a field.\n";
        return false;
    }

    coord = static_cast<unsigned>(coordRound);
    return true;
}

bool Board::addStone(int xPx, int yPx, bool black, SDL_Renderer* renderer) {
    unsigned xTrafo, yTrafo;
    if(pixelToCoord(xPx, xTrafo) && pixelToCoord(yPx, yTrafo)) {
        if(xTrafo >= m_nodes || yTrafo >= m_nodes) {
            std::cerr << "Invalid coordinates for board size.\n";
            return false;
        }

        const auto boardId = coordToId(xTrafo, yTrafo);
        if(m_board[boardId] != 0) {
            std::cerr << "Board position already occupied\n";
            return false;
        }

        m_board[boardId] = (black ? 1 : -1);
        drawStone(xTrafo, yTrafo, black ? 1 : -1, renderer);

        return true;
    }

    return false;
}