#include "testDataHelpers.hpp"

#include "core/serializer.hpp"
#include <fstream>
#include <gtest/gtest.h>

namespace tengen::vision::core {
namespace gtest {

static StoneState toStoneState(const Board::Stone stone) {
	switch (stone) {
	case Board::Stone::Black:
		return StoneState::Black;
	case Board::Stone::White:
		return StoneState::White;
	case Board::Stone::Empty:
		return StoneState::Empty;
	}
	return StoneState::Empty;
}

static std::vector<cv::Point2f> parsePoints(const nlohmann::json& array) {
	std::vector<cv::Point2f> points;
	points.reserve(array.size());
	for (const auto& p: array) {
		points.emplace_back(p.at(0).get<float>(), p.at(1).get<float>());
	}
	return points;
}


TestResult runPipeline(const std::filesystem::path& imgPath) {
	std::cout << "Running test: " << imgPath.string() << '\n';

	cv::Mat image = cv::imread(imgPath.string());
	EXPECT_FALSE(image.empty());

	// Warp image roughly around the board.
	WarpResult warped = warpToBoard(image);
	EXPECT_FALSE(warped.imageB0.empty());
	EXPECT_FALSE(warped.H0.empty());

	// Properly construct the board geometry.
	const BoardGeometry geometry = analyseGeometry(warped);
	EXPECT_TRUE(isValidGeometry(geometry));
	RectifiedBoard rectified = transformImage(image, geometry);
	EXPECT_TRUE(isValidRectifiedBoard(rectified));

	// Find the stones on the board.
	StoneResult stoneRes = analyseBoard(rectified);
	EXPECT_TRUE(stoneRes.success);
	EXPECT_EQ(stoneRes.stones.size(), rectified.geometry.intersections.size());

	return {warped, rectified, stoneRes};
}


Board loadExpectedBoard(const std::filesystem::path& imagePath) {
	Board expected(0u);
	std::filesystem::path txtPath = imagePath;
	txtPath.replace_extension(".txt");
	EXPECT_TRUE(readBoard(txtPath, expected)) << txtPath.string();
	return expected;
}

void expectStonesMatchBoard(const std::vector<StoneState>& stones, unsigned boardSize, const Board& expected) {
	ASSERT_EQ(stones.size(), static_cast<std::size_t>(boardSize) * boardSize);
	ASSERT_EQ(expected.size(), boardSize);

	for (unsigned x = 0; x < boardSize; ++x) {
		for (unsigned y = 0; y < boardSize; ++y) {
			const std::size_t index = static_cast<std::size_t>(x) * boardSize + y;
			EXPECT_EQ(stones[index], toStoneState(expected.get({x, y}))) << "Mismatch at (" << x << ", " << y << ")";
		}
	}
}

GeometryGroundTruth loadGeometryGroundTruth(const std::filesystem::path& imagePath) {
	GeometryGroundTruth truth{};

	std::filesystem::path jsonPath = imagePath;
	jsonPath.replace_extension(".json");

	std::ifstream file(jsonPath);
	EXPECT_TRUE(file.is_open()) << jsonPath.string();
	if (!file.is_open())
		return truth;

	const nlohmann::json j = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
	EXPECT_FALSE(j.is_discarded()) << "Invalid JSON: " << jsonPath.string();

	truth.boardSize    = j.value("boardSize", 0u);
	truth.boardCorners = parsePoints(j.at("boardCorners"));
	truth.gridCorners  = parsePoints(j.at("gridCorners"));
	return truth;
}

void expectPointsMatch(const std::vector<cv::Point2f>& expected, const std::vector<cv::Point2f>& actual, float tolerance, std::string_view context) {
	ASSERT_EQ(expected.size(), actual.size()) << context;

	std::vector<std::size_t> perm(actual.size());
	std::iota(perm.begin(), perm.end(), std::size_t{0});

	std::vector<std::size_t> bestPerm = perm;
	double bestCost                   = std::numeric_limits<double>::max();
	do {
		double cost = 0.0;
		for (std::size_t i = 0; i < expected.size(); ++i) {
			cost += cv::norm(expected[i] - actual[perm[i]]);
		}
		if (cost < bestCost) {
			bestCost = cost;
			bestPerm = perm;
		}
	} while (std::next_permutation(perm.begin(), perm.end()));

	for (std::size_t i = 0; i < expected.size(); ++i) {
		const double distance = cv::norm(expected[i] - actual[bestPerm[i]]);
		EXPECT_LE(distance, tolerance) << context << ": point " << i << " off by " << distance << "px (tolerance " << tolerance << "px)";
	}
}

} // namespace gtest
} // namespace tengen::vision::core
