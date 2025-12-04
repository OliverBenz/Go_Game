#pragma once

#include "core/types.hpp"
#include "core/board.hpp"

#include <vector>
#include <SDL.h>

namespace go {

// TODO: 
// - Cleanup function and variable names(board name redundant, specify pixel or type)
// - Cleanup board.size() / 2 -> replace with gameSize
// - Move all game logic to core library.
// - Wrap raw pointer in smart pointers?
class BoardRenderer{
public:
    BoardRenderer() = default;
    BoardRenderer(unsigned nodes, unsigned boardSizePx, SDL_Renderer* renderer);
    ~BoardRenderer();

    void draw(SDL_Renderer* renderer, const Board& board);

    //! Transforms pixel value to board coordinate.
    bool pixelToCoord(int px, unsigned& coord);

private:
    void drawBackground(SDL_Renderer* renderer);
    void drawStones(SDL_Renderer* renderer, const Board& board);
    void drawStone(Coord c, Board::FieldValue player, SDL_Renderer* renderer);

    static SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer);

private:
    // TODO: BoardLogic instance from Core
    // Then this class only handles GUI stuff. Logic in core.

    unsigned m_boardSize = 0;  //!< Pixels for the whole board (without coordinate text).
    unsigned m_stoneSize = 0;  //!< Pixel Radius of a stone.
    unsigned m_nodes = 0;

    // TODO: Check stones dont overlap by 1 pixel
    unsigned m_drawStepSize = m_stoneSize / 2;               //!< Half a stone offset from border
    unsigned m_coordStart   = m_drawStepSize;                //!< (x,y) starting coordinate of lines
    unsigned m_coordEnd     = m_boardSize - m_drawStepSize;  //!< (x,y) ending coordinate of lines

    SDL_Texture* m_textureBlack = nullptr;
    SDL_Texture* m_textureWhite = nullptr;
};

}