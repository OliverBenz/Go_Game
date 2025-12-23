#pragma once

#include "core/board.hpp"
#include "core/types.hpp"

#include <QImage>
#include <QPainter>

namespace go::ui {

class BoardRenderer {
public:
	BoardRenderer() = default;
	explicit BoardRenderer(unsigned nodes);

	void setBoardSizePx(unsigned boardSizePx);
	void draw(QPainter& painter, const Board& board) const;
	bool isReady() const;

	//! Try to convert pixel values to a board coordinate.
	bool pixelToCoord(int pX, int pY, Coord& coord) const;

private:
	//! Draw the board background.
	void drawBackground(QPainter& painter) const;
	//! Draw all stones given a board.
	void drawStones(QPainter& painter, const Board& board) const;
	//! Draw a single stone at a given index.
	void drawStone(QPainter& painter, Id x, Id y, Board::Value player) const;

	//! Transforms pixel value to board coordinate.
	bool pixelToCoord(int px, unsigned& coord) const;
	void updateMetrics(unsigned boardSizePx);
	void updateStoneTextures();

private:
	unsigned m_boardSize  = 0; //!< Pixels for the whole board (without coordinate text).
	unsigned m_stoneSize  = 0; //!< Pixel diameter of a stone.
	unsigned m_nodes      = 0; //!< Number of line intersection (Game board size).
	unsigned m_drawStepPx = 0; //!< Half a stone offset from border [px]
	unsigned m_coordStart = 0; //!< (x,y) starting coordinate of lines [px]
	unsigned m_coordEnd   = 0; //!< (x,y) ending coordinate of lines [px]

	QImage m_textureBlack;
	QImage m_textureWhite;
	QImage m_scaledBlack;
	QImage m_scaledWhite;
	bool m_ready = false; //!< Textures have been loaded.
};

} // namespace go::ui
