#pragma once

class BoardDetect {
public:
	BoardDetect();

	std::vector<std::pair<unsigned, bool>> process(const cv::Mat& img);

private:
	//! Adjust HSV range and update board mask
	void SetupBoardMask(const cv::Mat& img);

	//! Uses a mask and filters to detect the contour or the Go board in the image.
	cv::Mat DetectBoardContour(const cv::Mat& img);

	//! Warps the board to be square and cuts the image to board size.
	//! \note Calls VerifyBoardWarp to check propery executed.
	cv::Mat WarpAndCutBoard(const cv::Mat& img);

	//! Test cases: Board should be square (5% error allowed), Detect grid size (and verify against m_boardSize) and
	//! further checks
	bool VerifyBoardWarp(const cv::Mat& img);

	//! Applies a different mask that detects the stones on the board and whether they are white/black.
	//! \note Calls VerifyDetectStones to check properly executed.
	//! \returns Vector of stone locations and isBlackStone(true,false)
	std::vector<std::pair<cv::Vec3f, bool>> DetectStones(const cv::Mat& img);

	//! Sanity checks for stones. Just return true for now.
	bool VerifyDetectStones();

	//! Transforms image coordinates to Go board coordinates (0 <= boardId < m_boardSize*m_boardSize).
	std::vector<std::pair<unsigned, bool>> calcStonePositionIndex(const std::vector<std::pair<cv::Vec3f>>& stones);

private:
	unsigned m_boardSize = 19;

	cv::Mat m_boardMask;
	cv::Mat m_stoneMask;
}
