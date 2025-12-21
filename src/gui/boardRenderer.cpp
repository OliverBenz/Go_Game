#include "boardRenderer.hpp"

#include <QImageReader>
#include <QPainter>
#include <cassert>
#include <cmath>
#include <format>
#include <iostream>

namespace go::ui {

BoardRenderer::BoardRenderer(const unsigned nodes) : m_nodes(nodes) {
	m_ready = m_nodes > 0;

	const auto loadTexture = [this](const char* path, QImage& target) {
		QImageReader reader(path);
		reader.setAutoTransform(true);
		target = reader.read();
		if (target.isNull()) {
			std::cerr << std::format("Failed to load '{}': {}\n", path, reader.errorString().toStdString());
			m_ready = false;
		}
	};

	loadTexture(TEXTURE_BLACK, m_textureBlack);
	loadTexture(TEXTURE_WHITE, m_textureWhite);
}

void BoardRenderer::setBoardSizePx(const unsigned boardSizePx) {
	if (boardSizePx == 0 || m_nodes == 0) {
		return;
	}
	updateMetrics(boardSizePx);
	updateStoneTextures();
}

void BoardRenderer::updateMetrics(const unsigned boardSizePx) {
	m_boardSize  = boardSizePx / m_nodes * m_nodes;
	m_stoneSize  = m_boardSize / m_nodes;
	m_drawStepPx = m_stoneSize / 2;
	m_coordStart = m_drawStepPx;
	m_coordEnd   = m_boardSize > m_drawStepPx ? m_boardSize - m_drawStepPx : 0;
}

void BoardRenderer::updateStoneTextures() {
	if (!m_ready || m_stoneSize == 0) {
		return;
	}

	const QSize targetSize{static_cast<int>(m_stoneSize), static_cast<int>(m_stoneSize)};
	m_scaledBlack = m_textureBlack.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	m_scaledWhite = m_textureWhite.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void BoardRenderer::draw(QPainter& painter, const Board& board) const {
	if (!isReady()) {
		return;
	}

	drawBackground(painter);
	drawStones(painter, board);
}

bool BoardRenderer::isReady() const {
	return m_ready && m_boardSize > 0 && m_stoneSize > 0 && !m_scaledBlack.isNull() && !m_scaledWhite.isNull();
}

void BoardRenderer::drawBackground(QPainter& painter) const {
	static constexpr int LW = 2; //!< Line width for grid
	static const QColor background{220, 179, 92};

	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.fillRect(QRect{0, 0, static_cast<int>(m_boardSize), static_cast<int>(m_boardSize)}, background);

	painter.setPen(QPen(Qt::black, LW));
	const int effBoardWidth = static_cast<int>(m_coordEnd - m_coordStart);
	const int coordStart    = static_cast<int>(m_coordStart);
	const int coordEnd      = coordStart + effBoardWidth;
	for (unsigned i = 0; i != m_nodes; ++i) {
		const int offset = static_cast<int>(m_coordStart + i * m_stoneSize);
		painter.drawLine(coordStart, offset, coordEnd, offset);
		painter.drawLine(offset, coordStart, offset, coordEnd);
	}
	painter.restore();
}

void BoardRenderer::drawStone(QPainter& painter, Id x, Id y, const Board::Value player) const {
	if (!isReady()) {
		return;
	}

	const int drawX = static_cast<int>((m_coordStart - m_drawStepPx) + x * m_stoneSize);
	const int drawY = static_cast<int>((m_coordStart - m_drawStepPx) + y * m_stoneSize);
	const QRect dest{drawX, drawY, static_cast<int>(m_stoneSize), static_cast<int>(m_stoneSize)};

	const auto& texture = (player == Board::Value::Black) ? m_scaledBlack : m_scaledWhite;
	painter.drawImage(dest, texture);
}

void BoardRenderer::drawStones(QPainter& painter, const Board& board) const {
	for (Id i = 0; i != board.size(); ++i) {
		for (Id j = 0; j != board.size(); ++j) {
			if (board.getAt({i, j}) != Board::Value::Empty) {
				drawStone(painter, i, j, board.getAt({i, j}));
			}
		}
	}
}

bool BoardRenderer::pixelToCoord(int pX, int pY, Coord& coord) const {
	Id x, y;
	if (pixelToCoord(pX, x) && pixelToCoord(pY, y)) {
		coord = {x, y};
		return true;
	}
	return false;
}

bool BoardRenderer::pixelToCoord(const int px, unsigned& coord) const {
	static constexpr float TOLERANCE = 0.3f; // To avoid accidental placement of stones.

	if (m_stoneSize == 0) {
		return false;
	}

	const auto coordRel = static_cast<float>(px - static_cast<int>(m_coordStart)) /
	                      static_cast<float>(m_stoneSize); // Calculate board coordinate from pixel values.
	const auto coordRound = std::round(coordRel);          // Round to nearest coordinate.

	// Click has to be close enough to a point and on the board.
	if (std::abs(coordRound - coordRel) > TOLERANCE || coordRound < 0 || coordRound > static_cast<float>(m_nodes) - 1) {
		return false;
	}

	coord = static_cast<unsigned>(coordRound);
	return true;
}

} // namespace go::ui
