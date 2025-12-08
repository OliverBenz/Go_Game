#include "boardRenderer.hpp"

#include <SDL_image.h>
#include <cassert>
#include <cmath>
#include <format>
#include <iostream>

namespace go::sdl {

BoardRenderer::BoardRenderer(unsigned nodes, unsigned boardSizePx, SDL_Renderer* renderer)
    : m_boardSize{boardSizePx / nodes * nodes},
      m_stoneSize{m_boardSize / nodes}, // TODO: Could still be a pixel off due to rounding int division
      m_nodes(nodes) {
	// Texture macros set with CMake
	m_textureBlack = load_texture(TEXTURE_BLACK, renderer);
	m_textureWhite = load_texture(TEXTURE_WHITE, renderer);
	if (!m_textureWhite || !m_textureBlack) {
		return;
	}
}

BoardRenderer::~BoardRenderer() {
	SDL_DestroyTexture(m_textureBlack);
	SDL_DestroyTexture(m_textureWhite);
}

// TODO: Don't redraw background and past stones all the time.
// TODO: Always draw new stone individually and add function redrawBoard
void BoardRenderer::draw(const Board& board, SDL_Renderer* renderer) {
	drawBackground(renderer);
	drawStones(board, renderer);
}

SDL_Texture* BoardRenderer::load_texture(const char* path, SDL_Renderer* renderer) {
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

void BoardRenderer::drawBackground(SDL_Renderer* renderer) {
	static constexpr int LW         = 2; //!< Line width for grid
	static constexpr Uint8 COL_BG_R = 220;
	static constexpr Uint8 COL_BG_G = 179;
	static constexpr Uint8 COL_BG_B = 92;

	assert(m_coordEnd > m_coordStart);
	static const int effBoardWidth = static_cast<int>(m_coordEnd - m_coordStart); //!< Board width between outer lines


	SDL_Rect dest_board{.x = 0, .y = 0, .w = static_cast<int>(m_boardSize), .h = static_cast<int>(m_boardSize)};
	SDL_SetRenderDrawColor(renderer, COL_BG_R, COL_BG_G, COL_BG_B, 255);
	SDL_RenderFillRect(renderer, &dest_board);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	for (unsigned i = 0; i != m_nodes; ++i) {
		SDL_Rect dest_line{.x = static_cast<int>(m_coordStart),
		                   .y = static_cast<int>(m_coordStart + i * m_stoneSize),
		                   .w = effBoardWidth,
		                   .h = LW};
		SDL_RenderFillRect(renderer, &dest_line);
	}
	for (unsigned i = 0; i != m_nodes; ++i) {
		SDL_Rect dest_line{.x = static_cast<int>(m_coordStart + i * m_stoneSize),
		                   .y = static_cast<int>(m_coordStart),
		                   .w = LW,
		                   .h = effBoardWidth};
		SDL_RenderFillRect(renderer, &dest_line);
	}
}

void BoardRenderer::drawStone(Id x, Id y, Board::FieldValue player, SDL_Renderer* renderer) {
	assert(m_coordStart >= m_drawStepSize);

	SDL_Rect dest;
	dest.w = static_cast<int>(m_stoneSize);
	dest.h = static_cast<int>(m_stoneSize);
	dest.x = static_cast<int>((m_coordStart - m_drawStepSize) + x * m_stoneSize);
	dest.y = static_cast<int>((m_coordStart - m_drawStepSize) + y * m_stoneSize);

	SDL_RenderCopy(renderer, (player == Board::FieldValue::Black ? m_textureBlack : m_textureWhite), nullptr, &dest);
}

void BoardRenderer::drawStones(const Board& board, SDL_Renderer* renderer) {
	for (Id i = 0; i != board.size(); ++i) {
		for (Id j = 0; j != board.size(); ++j) {
			if (board.getAt({i, j}) != Board::FieldValue::None) {
				drawStone(i, j, board.getAt({i, j}), renderer);
			}
		}
	}
}


bool BoardRenderer::pixelToCoord(int pX, int pY, Coord& coord) {
	Id x, y;
	if (pixelToCoord(pX, x) && pixelToCoord(pY, y)) {
		coord = {x, y};
		return true;
	}
	return false;
}

bool BoardRenderer::pixelToCoord(int px, unsigned& coord) {
	static constexpr float TOLERANCE = 0.3f; // To avoid accidental placement of stones.

	const auto coordRel = static_cast<float>(px - static_cast<int>(m_coordStart)) /
	                      static_cast<float>(m_stoneSize); // Calculate board coordinate from pixel values.
	const auto coordRound = std::round(coordRel);          // Round to nearest coordinate.

	// Click has to be close enough to a point and on the board.
	if (std::abs(coordRound - coordRel) > TOLERANCE || coordRound < 0 || coordRound > static_cast<float>(m_nodes) - 1) {
		std::cerr << "Could not assign mouse coordinats to a field.\n";
		return false;
	}

	coord = static_cast<unsigned>(coordRound);
	return true;
}

} // namespace go::sdl
