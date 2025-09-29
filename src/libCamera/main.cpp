#include <opencv2/opencv.hpp>
#include <iostream>

int hmin=0, smin=89, vmin=178;
int hmax=161, smax=220, vmax=226;

//! Uses default HSV space that manually was found to provide a good contour
//! of the board and stones in simple imaging setups.
class MaskContour{
public:
    // Apply HSV mask to one image
    cv::Mat process(const cv::Mat& img) {
        cv::Mat hsv, mask, result;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);
        cv::cvtColor(mask, result, cv::COLOR_GRAY2BGR); // so we can stack consistently
        return result;
    }

private:
    static constexpr int hmin=13, smin=44, vmin=194;
    static constexpr int hmax=37, smax=197, vmax=255;

    //! Shifting vmin around the base by this amound shoud allow to find most boards.
    static constexpr int vminRange = 20;
};

class MaskTesterHSV{
public:
    MaskTesterHSV(){
        // Create trackbars
        namedWindow("Mask", cv::WINDOW_AUTOSIZE);
        cv::createTrackbar("H Min", "Mask", &hmin, 179);
        cv::createTrackbar("H Max", "Mask", &hmax, 179);
        cv::createTrackbar("S Min", "Mask", &smin, 255);
        cv::createTrackbar("S Max", "Mask", &smax, 255);
        cv::createTrackbar("V Min", "Mask", &vmin, 255);
        cv::createTrackbar("V Max", "Mask", &vmax, 255);
    }
    // Apply HSV mask to one image
    cv::Mat processSimpleMask(const cv::Mat& img) {
        cv::Mat hsv, mask, result;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);
        cv::cvtColor(mask, result, cv::COLOR_GRAY2BGR); // so we can stack consistently
        return result;
    }

    cv::Mat processAndWarp(const cv::Mat& img) {
        cv::Mat hsv, mask;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(hmin, smin, vmin),
                        cv::Scalar(hmax, smax, vmax), mask);

        // --- find board contour ---
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (contours.empty()) {
            return img.clone(); // fallback
        }

        // largest contour = board
        double maxArea = 0;
        int maxIdx = -1;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                maxIdx = (int)i;
            }
        }

        std::vector<cv::Point> approx;
        cv::approxPolyDP(contours[maxIdx], approx,
                        0.02 * cv::arcLength(contours[maxIdx], true), true);

        if (approx.size() != 4) {
            // not a quad, just show mask overlay
            cv::Mat debug;
            cv::cvtColor(mask, debug, cv::COLOR_GRAY2BGR);
            cv::drawContours(debug, contours, maxIdx, cv::Scalar(0,255,0), 2);
            return debug;
        }

        // --- order the 4 corners consistently ---
        // compute centroid
        cv::Point2f center(0,0);
        for (auto& p : approx)
            center +=  cv::Point2f(p);
        center *= (1.0 / approx.size());

        std::vector<cv::Point2f> ordered(4);
        for (auto& p : approx) {
            if      (p.x < center.x && p.y < center.y) ordered[0] = p; // top-left
            else if (p.x > center.x && p.y < center.y) ordered[1] = p; // top-right
            else if (p.x > center.x && p.y > center.y) ordered[2] = p; // bottom-right
            else                                       ordered[3] = p; // bottom-left
        }

        // --- target rectangle size ---
        int outSize = 800; // you can pick board resolution
        std::vector<cv::Point2f> target = {
            {0,0}, {outSize-1,0}, {outSize-1,outSize-1}, {0,outSize-1}
        };

        // --- perspective transform ---
        cv::Mat M = cv::getPerspectiveTransform(ordered, target);
        cv::Mat warped;
        cv::warpPerspective(img, warped, M, cv::Size(outSize, outSize));

        return warped;
    }


    // TODO: Stone detection is bad. I need a different mask to find stones and their colour.
    cv::Mat processWithStones(const cv::Mat& img) {
        cv::Mat warped = processAndWarp(img);
        auto stones = detectStones(warped);
        cv::Mat withStones = drawStones(warped, stones);
    
        return withStones;
    }

    std::vector<cv::Vec3f> detectStones(const cv::Mat& warpedBoard) {
        cv::Mat gray;
        cv::cvtColor(warpedBoard, gray, cv::COLOR_BGR2GRAY);
        cv::medianBlur(gray, gray, 5);

        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1,
                        20,   // minDist between stone centers (tune: ~cell size)
                        100, 30, 10, 30); // param1, param2, minRadius, maxRadius

        return circles;
    }
    
    cv::Mat drawStones(const cv::Mat& img, const std::vector<cv::Vec3f>& stones) {
        cv::Mat result = img.clone();
        for (size_t i = 0; i < stones.size(); i++) {
            cv::Point center(cvRound(stones[i][0]), cvRound(stones[i][1]));
            int radius = cvRound(stones[i][2]);
            // Circle outline
            cv::circle(result, center, radius, cv::Scalar(0, 255, 0), 2);
            // Center dot
            cv::circle(result, center, 2, cv::Scalar(0, 0, 255), 3);
        }
        return result;
    }


    int hmin=13, smin=44,  vmin=187; // or vmin 186
    int hmax=37, smax=197, vmax=255;
};



cv::Mat makeGrid(const std::vector<cv::Mat>& images) {
    if (images.size() != 6) {
        std::cerr << "Need exactly 6 images\n";
        return cv::Mat();
    }
    
    // Resize all to the same size for neat layout
    cv::Size sz(320, 240); // pick a display size
    std::vector<cv::Mat> resized;
    for (auto& img : images) {
        cv::Mat r;
        cv::resize(img, r, sz);
        resized.push_back(r);
    }

    // Row 1
    cv::Mat row1;
    cv::hconcat(std::vector<cv::Mat>{resized[0], resized[1], resized[2]}, row1);
    
    // Row 2
    cv::Mat row2;
    cv::hconcat(std::vector<cv::Mat>{resized[3], resized[4], resized[5]}, row2);
    
    // Final grid
    cv::Mat grid;
    cv::vconcat(row1, row2, grid);
    
    return grid;
}

class BoardDetect {
public:
    BoardDetect() {
        // Initial setup with manual values
        SetupBoardMask();
        SetupStoneMask();
    }

    std::vector<std::pair<unsigned, bool>> process(const cv::Mat& img){
        auto res = detectBoardContour();
        res = warpAndCutBoard(res);
        
        auto stones = detectStones(res);
        return calcStonePositionIndex(stones);
    }

private:
    //! Adjust HSV range and update board mask
    void SetupBoardMask(const cv::Mat& img) {        
        // HSV Range will be properly set at
        static int m_hmin=13, m_smin=44,  m_vmin=187;
        static int m_hmax=37, m_smax=197, m_vmax=255;

        // Histogram equalization or CLAHE on V channel improves detection under variable lighting.
    }

    //! Uses a mask and filters to detect the contour or the Go board in the image.
    cv::Mat DetectBoardContour(const cv::Mat& img) {
        // Consider morphological operations (MORPH_CLOSE / MORPH_OPEN) to clean up the mask before finding contours.

        // If fails       -> SetupBoardMask and try again (maybe lighting changed)
        // If still fails -> Gracefully fail
    }

    //! Warps the board to be square and cuts the image to board size.
    //! \note Calls VerifyBoardWarp to check propery executed.
    cv::Mat WarpAndCutBoard(const cv::Mat& img) {
        // Im assuming a quadrilateral. If the contour has extra noise, approxPolyDP may produce >4 points.
        //  -> If more than 4 points, consider convex hull or polygon simplification to reliably pick corners.
    }

    //! Test cases: Board should be square (5% error allowed), Detect grid size (and verify against m_boardSize) and further checks
    bool VerifyBoardWarp(const cv::Mat& img);

    //! Applies a different mask that detects the stones on the board and whether they are white/black.
    //! \note Calls VerifyDetectStones to check properly executed.
    //! \returns Vector of stone locations and isBlackStone(true,false)
    std::vector<std::pair<cv::Vec3f, bool>> DetectStones(const cv::Mat& img){
        // Consider masking only inside board area before stone detection. This reduces false positives.


        // Robust way to distinguish black vs white stones:
        // Use V (value) channel in HSV or grayscale mean intensity.
        // Normalize by local background (to account for shadows).
        // Optional: adaptive threshold or simple learning-based method if lighting varies a lot.

        // Optional: color code stones by detection confidence.
    }

    //! Sanity checks for stones. Just return true for now.  
    bool VerifyDetectStones();

    //! Transforms image coordinates to Go board coordinates (0 <= boardId < m_boardSize*m_boardSize).
    std::vector<std::pair<, bool>> calcStonePositionIndex(const std::vector<std::pair<cv::Vec3f>>& stones) {
        // Board warp may have small scaling errors (both physical warp and image warp)
        // Stones not perfectly centered in intersections
    }

private:
    unsigned m_boardSize = 19;

    cv::Mask m_boardMask;
    cv::Maks m_stoneMask;
}


int main() {
    // Load your 6 reference images
    std::vector<cv::Mat> images;
    for (auto i = 1; i != 7; ++i) {
        cv::Mat img = cv::imread("/home/oliver/Data/dev/Go_Game/tests/img/boards_easy/angle_"+ std::to_string(i) + ".jpeg");
        if (img.empty()) {
            std::cerr << "Could not read " << i << std::endl;
            return -1;
        }
        images.push_back(img);
    }

    MaskTesterHSV testMask;
    MaskContour contourMask;
    while (true) {
        std::vector<cv::Mat> processed;
        for (auto& img : images) {
            processed.push_back(testMask.processWithStones(img));
        }
        cv::Mat grid = makeGrid(processed);
        cv::imshow("Masks", grid);

        if (cv::waitKey(30) == 27)
            break; // Esc to quit
    }

    return 0;
}
