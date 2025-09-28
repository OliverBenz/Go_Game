#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

int main() {
    Mat frame = imread("goboard.jpg"); // test image

    if (frame.empty()) {
        cout << "Could not read the image" << endl;
        return -1;
    }

    imshow("Go Board", frame);
    waitKey(0);
    return 0;
}
