#pragma once

#include <opencv2/opencv.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

class BoardDetect {
public:
	explicit BoardDetect(std::string debugDir = "camera_debug");

	std::vector<std::pair<unsigned, bool>> process(const cv::Mat& img);

	unsigned boardSize() const;

private:
	//! Persist a debug image with a numbered prefix (e.g. 0_original.png).
	void saveDebugImage(int step, const std::string& name, const cv::Mat& img) const;

	//! Create a board mask robust to lighting changes.
	cv::Mat buildBoardMask(const cv::Mat& img) const;

	//! Uses a mask and filters to detect the contour or the Go board in the image.
	bool findBoardCorners(const cv::Mat& mask, std::array<cv::Point2f, 4>& corners, cv::Mat& debug) const;

	//! Validate board contour geometry (area + aspect).
	bool validateBoardCorners(const std::array<cv::Point2f, 4>& corners, const cv::Size& imgSize) const;

	//! Warps the board to be square and cuts the image to board size.
	cv::Mat warpBoard(const cv::Mat& img,
	                  const std::array<cv::Point2f, 4>& corners,
	                  int outSize,
	                  float scale,
	                  cv::Mat& debug) const;

	//! Detects grid lines and estimates board size (9/13/19).
	bool detectGridLines(const cv::Mat& warped, std::vector<float>& xLines, std::vector<float>& yLines, cv::Mat& debug) const;

	//! Validate grid line distribution (line count + spacing).
	bool validateGridLines(const std::vector<float>& xLines, const std::vector<float>& yLines) const;

	//! Choose nearest supported board size based on detected line counts.
	unsigned estimateBoardSize(std::size_t xCount, std::size_t yCount) const;

	//! Applies a different mask that detects the stones on the board and whether they are white/black.
	std::vector<cv::Vec3f> detectStones(const cv::Mat& warped, float cellSize, cv::Mat& debug) const;

	//! Validate stone detections against plausible counts.
	bool validateStoneDetections(std::size_t count) const;

	//! Classify a stone as black/white based on local intensity.
	bool classifyStone(const cv::Mat& gray, const cv::Vec3f& stone, bool& isBlack) const;

	//! Transforms image coordinates to Go board coordinates (0 <= boardId < m_boardSize*m_boardSize).
	std::vector<std::pair<unsigned, bool>> mapStonePositions(const std::vector<cv::Vec3f>& stones, const std::vector<bool>& colors, const cv::Size& boardSizePx,
	                                                         const std::vector<float>& xLines, const std::vector<float>& yLines) const;

private:
	unsigned m_boardSize = 19;
	std::string m_debugDir;
};
