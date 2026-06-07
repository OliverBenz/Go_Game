#pragma once

#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"

#include <opencv2/opencv.hpp>
#include <filesystem>

namespace tengen::vision::devtools {

//! Helper to create a cv::Mat of a go board.
class Board {
public:
	Board(unsigned size);
	void setStone(unsigned x, unsigned y, core::StoneState s) {
		// TODO: We have to keep track of the homography.
		// m_boardImage = (m_homography \circ Place Stone \circ m_homography^{-1})
	}

	cv::Mat& getImage();                              //!< Returns the produced board image.
	bool save(std::filesystem::path const& fileName); //!< Save the board image to the drive.
	void show();	                                  //!< Show the board image in popup window.

private:
	cv::Mat m_homography;
	cv::Mat m_boardImage;
};

/*! Turn a board into an image.
 * \param [in] board The board date.
 * \param [in] spacingPx Pixel spacing between consecutive grid lines.
 * \param [in] H Optional transformation homography to warp the board in space.
 */
//cv::Mat createImage(core::Board board, unsigned spacingPx, cv::Mat H);

// TODO: Remove functions below and use one of the options from above (or provide both..).

//! Draw a synthetic board image in canonical board coordinates.
cv::Mat makeCanonicalBoardImage(unsigned boardSize, int sidePx);

//! Warp a canonical board image into a scene using a destination quadrilateral.
cv::Mat warpBoardToScene(const cv::Mat& board, cv::Size sceneSize, const std::array<cv::Point2f, 4>& dstQuad);

//! Build a synthetic image with a clearly visible board outline but no internal grid.
cv::Mat makeOutlineOnlySyntheticScene();

//! Build a high-contrast synthetic image with a board that almost touches all image borders.
cv::Mat makeFullFrameSyntheticScene();

//! Create a synthetic, perfectly rectified board with evenly spaced intersections.
core::RectifiedBoard makeSyntheticBoard(unsigned N, double spacingPx, const cv::Scalar& woodBgr);

//! Draw a filled stone at a given grid coordinate (gx,gy).
void drawStone(core::RectifiedBoard& g, unsigned gx, unsigned gy, core::StoneState s);

} // namespace tengen::vision::core
