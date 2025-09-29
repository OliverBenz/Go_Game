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
    static constexpr int hmin=13, smin=44, vmin=172;
    static constexpr int hmax=37, smax=197, vmax=255;
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
    cv::Mat process(const cv::Mat& img) {
        cv::Mat hsv, mask, result;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
        cv::inRange(hsv, cv::Scalar(hmin, smin, vmin), cv::Scalar(hmax, smax, vmax), mask);
        cv::cvtColor(mask, result, cv::COLOR_GRAY2BGR); // so we can stack consistently
        return result;
    }

    int hmin=13, smin=44,  vmin=172;
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
            processed.push_back(testMask.process(img));
        }
        cv::Mat grid = makeGrid(processed);
        cv::imshow("Masks", grid);

        if (cv::waitKey(30) == 27)
            break; // Esc to quit
    }

    return 0;
}
