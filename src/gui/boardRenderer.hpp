#pragma once

#include "core/board.hpp"
#include "core/types.hpp"

#include <SDL.h>
#include <vector>

namespace go::sdl {

// TODO:
// - Cleanup function and variable names(board name redundant, specify pixel or type)
// - Wrap raw pointer in smart pointers?
class BoardRenderer {
public:
	BoardRenderer() = default;
	BoardRenderer(unsigned nodes, unsigned boardSizePx, SDL_Renderer* renderer);
	~BoardRenderer();

	void draw(const Board& board, SDL_Renderer* renderer);

	//! Try to convert pixel values to a board coordinate.
	bool pixelToCoord(int pX, int pY, Coord& coord);

private:
	void drawBackground(SDL_Renderer* renderer);
	void drawStones(const Board& board, SDL_Renderer* renderer);
	void drawStone(Id x, Id y, Board::FieldValue player, SDL_Renderer* renderer);

	//! Transforms pixel value to board coordinate.
	bool pixelToCoord(int px, unsigned& coord);

	static SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer);

private:
	unsigned m_boardSize = 0; //!< Pixels for the whole board (without coordinate text).
	unsigned m_stoneSize = 0; //!< Pixel Radius of a stone.
	unsigned m_nodes     = 0; //!< Number of line intersection (Game board size).

	// TODO: Check stones dont overlap by 1 pixel
	unsigned m_drawStepSize = m_stoneSize / 2;              //!< Half a stone offset from border [px]
	unsigned m_coordStart   = m_drawStepSize;               //!< (x,y) starting coordinate of lines [px]
	unsigned m_coordEnd     = m_boardSize - m_drawStepSize; //!< (x,y) ending coordinate of lines [px]

	SDL_Texture* m_textureBlack = nullptr;
	SDL_Texture* m_textureWhite = nullptr;
};

} // namespace go::sdl
