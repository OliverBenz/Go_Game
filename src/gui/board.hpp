#pragma once

#include <vector>
#include <SDL.h>


// TODO: 
// - Cleanup function and variable names(board name redundant, specify pixel or type)
// - Cleanup board.size() / 2 -> replace with gameSize
// - Move all game logic to core library.
// - Wrap raw pointer in smart pointers?
class Board{
public:
    Board() = default;
    Board(int nodes, int boardSizePx, SDL_Renderer* renderer);
    ~Board();

    void draw(SDL_Renderer* renderer);

    //! TODO: Remove this function. In GUI, we always place stones through mouse coordinates.
    //! Official coordinates (A1, etc)
    bool addStone(char x, int y, bool black, SDL_Renderer* renderer);

    //! Add stone at the specified window position (in pixels).
    //! \returns False if click could not be assigned to a board position.
    bool addStone(int xPx, int yPx, bool black, SDL_Renderer* renderer);

private:
    void drawBackground(SDL_Renderer* renderer);
    void drawStones(SDL_Renderer* renderer);
    void drawStone(int x, int y, int player, SDL_Renderer* renderer);

    //! (x,y) \mapsto i \in [0, nodes-1]
    int coordToId(int x, int y);

    //! Transforms pixel value to board coordinate.
    bool pixelToCoord(int px, int& coord);

    static SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer);

private:
    // TODO: BoardLogic instance from Core
    // Then this class only handles GUI stuff. Logic in core.

    int m_boardSize = 0;  //!< Pixels for the whole board (without coordinate text).
    int m_stoneSize = 0;  //!< Pixel Radius of a stone.
    int m_nodes = 0;

    std::vector<char> m_board;

    // TODO: Check stones dont overlap by 1 pixel
    int m_drawStepSize = m_stoneSize / 2;               //!< Half a stone offset from border
    int m_coordStart   = m_drawStepSize;                //!< (x,y) starting coordinate of lines
    int m_coordEnd     = m_boardSize - m_drawStepSize;  //!< (x,y) ending coordinate of lines

    SDL_Texture* m_textureBlack = nullptr;
    SDL_Texture* m_textureWhite = nullptr;
};
