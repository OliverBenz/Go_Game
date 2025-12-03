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


    int hmin=13, smin=44,  vmin=84; // or vmin 186
    int hmax=37, smax=166, vmax=255;
};

cv::Mat makeGrid(const std::vector<cv::Mat>& images) {
    if (images.size() != 6) {
        std::cerr << "Need exactly 6 images\n";
        return cv::Mat();
    }
    
    // Resize all to the same size for neat layout
    cv::Size sz(400, 400); // pick a display size
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


int main() {
    // Load your 6 reference images
    std::vector<cv::Mat> images;
    for (auto i = 2; i != 8; ++i) {
        cv::Mat img = cv::imread("/home/oliver/Data/dev/Go_Game/tests/img/boards_easy/angle_"+ std::to_string(i) + ".jpeg");
        if (img.empty()) {
            std::cerr << "Could not read " << i << std::endl;
            return -1;
        }
        images.push_back(img);
    }

    MaskTesterHSV mask;
    // MaskContour mask;
    // BoardDetect boardHandler;
    while (true) {
        std::vector<cv::Mat> processed;
        for (auto& img : images) {
            processed.push_back(mask.processAndWarp(img));
        }
        cv::Mat grid = makeGrid(processed);
        cv::imshow("Masks", grid);

        if (cv::waitKey(30) == 27)
            break; // Esc to quit
    }

    return 0;
}
