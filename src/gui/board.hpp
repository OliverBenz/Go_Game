#pragma once

#include <vector>
#include <iostream>
#include <SDL.h>
#include <SDL_image.h>


// TODO: 
// - Cleanup function and variable names(board name redundant, specify pixel or type)
// - Cleanup board.size() / 2 -> replace with gameSize
// - Move all game logic to core library.
class Board{
public:
    Board(int nodes, int boardSizePx, SDL_Renderer* renderer);
    ~Board();

    void draw(SDL_Renderer* renderer);

    //! Official coordinates (A1, etc)
    bool addStone(char x, int y, bool black, SDL_Renderer* renderer);
    
private:
    void drawBackground(SDL_Renderer* renderer);
    void drawStones(SDL_Renderer* renderer);
    void drawStone(int x, int y, int player, SDL_Renderer* renderer);

    //! x,y \in [0, nodes-1]
    int coordToId(int x, int y);

    static SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer);

private:
    // TODO: BoardLogic instance from Core
    // Then this class only handles GUI stuff. Logic in core.

    int boardSize = 0;  //!< Pixels for the whole board (without coordinate text).
    int stoneSize = 0;  //!< Pixel Radius of a stone.
    int nodes = 0;

    std::vector<char> board;

    int drawStepSize = stoneSize / 2;             //!< Half a stone offset from border
    int coordStart   = drawStepSize;              //!< (x,y) starting coordinate of lines
    int coordEnd     = boardSize - drawStepSize;  //!< (x,y) ending coordinate of lines

    SDL_Texture* textureBlack = nullptr;
    SDL_Texture* textureWhite = nullptr;
};
