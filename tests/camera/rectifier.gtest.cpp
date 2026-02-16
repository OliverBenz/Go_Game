#include "camera/rectifier.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <filesystem>

namespace go::gtest {

TEST(Rectifier, RectifyImage) {
    // // Load Image
	// cv::Mat image9  = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_9.jpeg");
	// cv::Mat image13 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_13.jpeg");
	// cv::Mat image19 = cv::imread(std::filesystem::path(PATH_TEST_IMG) / "straight_easy/size_19.jpeg");
	
	// auto rect9  = camera::rectifyImage(image9);
	// auto rect13 = camera::rectifyImage(image13);
	// auto rect19 = camera::rectifyImage(image19);
}


}
