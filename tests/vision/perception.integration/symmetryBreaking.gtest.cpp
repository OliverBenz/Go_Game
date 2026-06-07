#include "vision/core/boardFinder.hpp"
#include "vision/core/gridFinder.hpp"
#include "vision/core/stoneFinder.hpp"
#include "vision/vision.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tengen::vision {
namespace gtest {
namespace {

enum class D4 : unsigned char { Id, Rot90, Rot180, Rot270, FlipX, FlipY, Diag, AntiDiag };

constexpr std::array<D4, 8> D4_GROUP = {
        D4::Id, D4::Rot90, D4::Rot180, D4::Rot270, D4::FlipX, D4::FlipY, D4::Diag, D4::AntiDiag,
};

struct DetectedStone {
	unsigned boardSize;
	Coord coord;
	core::StoneState colour;
};

//! Apply a symmetry transformation on a board coordinate.
Coord transformCoord(const Coord c, const D4 g, const unsigned boardSize) {
	switch (g) {
	case D4::Id:
		return c;
	case D4::Rot90:
		return {boardSize - 1u - c.y, c.x};
	case D4::Rot180:
		return {boardSize - 1u - c.x, boardSize - 1u - c.y};
	case D4::Rot270:
		return {c.y, boardSize - 1u - c.x};
	case D4::FlipX:
		return {boardSize - 1u - c.x, c.y};
	case D4::FlipY:
		return {c.x, boardSize - 1u - c.y};
	case D4::Diag:
		return {c.y, c.x};
	case D4::AntiDiag:
		return {boardSize - 1u - c.y, boardSize - 1u - c.x};
	}
	return c;
}

//! Check black of white stone.
inline bool isStone(const core::StoneState state) {
	return state == core::StoneState::Black || state == core::StoneState::White;
}

//! Run the image detection pipeline and return the 1 stone on the board.
//! \returns Null for failure in pipeline or not exactly 1 stone found.
std::optional<DetectedStone> detectSingleStone(const std::filesystem::path& imagePath) {
	// Load image
	const cv::Mat image = cv::imread(imagePath.string());
	if (image.empty()) {
		return std::nullopt;
	}

	// Detection Pipeline
	const core::WarpResult warped = core::warpToBoard(image);
	if (!core::isValidBoard(warped)) {
		return std::nullopt;
	}

	const core::BoardGeometry geometry   = core::analyseGeometry(warped);
	const core::RectifiedBoard rectified = core::transformImage(image, geometry);
	if (!core::isValidRectifiedBoard(rectified)) {
		return std::nullopt;
	}

	const core::StoneResult result = core::analyseBoard(rectified);
	if (!result.success || result.stones.size() != rectified.geometry.intersections.size()) {
		return std::nullopt;
	}

	// Find exactly 1 stone.
	std::size_t stoneIndex         = result.stones.size();
	core::StoneState detectedStone = core::StoneState::Empty;
	for (std::size_t i = 0; i < result.stones.size(); ++i) {
		if (!isStone(result.stones[i])) {
			continue;
		}
		if (stoneIndex != result.stones.size()) {
			return std::nullopt; // More than 1 stone found
		}
		stoneIndex    = i;
		detectedStone = result.stones[i];
	}

	if (stoneIndex == result.stones.size()) {
		return std::nullopt; // No stone found
	}

	return DetectedStone{
	        rectified.geometry.boardSize,
	        Coord{
	                static_cast<unsigned>(stoneIndex / rectified.geometry.boardSize),
	                static_cast<unsigned>(stoneIndex % rectified.geometry.boardSize),
	        },
	        detectedStone,
	};
}

//! List of coordinates contains a specific one.
inline bool containsCoord(const std::vector<Coord>& coords, const Coord c) {
	return std::any_of(coords.begin(), coords.end(), [&](const Coord value) { return value.x == c.x && value.y == c.y; });
}

//! Return the D_4 gauge orbit of a given coordinate: O_c^{D_4} = { g \rhd c | g \in D_4}
std::vector<Coord> d4Orbit(const Coord placedCoord, const unsigned boardSize) {
	std::vector<Coord> orbit;
	orbit.reserve(D4_GROUP.size());
	for (const D4 g: D4_GROUP) {
		const Coord mapped = transformCoord(placedCoord, g, boardSize);
		if (!containsCoord(orbit, mapped)) {
			orbit.push_back(mapped);
		}
	}
	return orbit;
}

//! Return the a board coordinate not in the gauge orbit of some c: c' \notin O_c^{D_4}
std::optional<Coord> pickOutsideOrbit(const unsigned boardSize, const std::vector<Coord>& orbit) {
	for (unsigned x = 0; x < boardSize; ++x) {
		for (unsigned y = 0; y < boardSize; ++y) {
			const Coord c{x, y};
			if (!containsCoord(orbit, c)) {
				return c;
			}
		}
	}

	assert(false); // |D_4| = 8, min(BoardSize) = 9 -> 91 Coords. Impossible to not find a c' \notin O_c^{D_4}
	return std::nullopt;
}

//! Construct full path of test image given fileName.
std::filesystem::path setupImage(const std::string_view fileName) {
	return std::filesystem::path(PATH_TEST_IMG) / "setup" / std::string(fileName);
}

} // namespace

TEST(Perception, Setup_SymmetryBreaking_D4Orbit) {
	static constexpr std::array<std::string_view, 8> BLACK_CASES = {
	        "C2_1.png", "C2_2.png", "C2_3.png", "C2_4.png", "E3_1.png", "E3_2.png", "E3_3.png", "E3_4.png",
	};

	for (const std::string_view fileName: BLACK_CASES) {
		const auto imagePath = setupImage(fileName);
		const auto stone     = detectSingleStone(imagePath);
		ASSERT_TRUE(stone.has_value()) << imagePath.string();
		ASSERT_EQ(stone->colour, core::StoneState::Black) << imagePath.string();

		const std::vector<Coord> orbit = d4Orbit(stone->coord, stone->boardSize);
		ASSERT_GT(orbit.size(), 1u) << imagePath.string();

		const auto outsideOrbit = pickOutsideOrbit(stone->boardSize, orbit);
		ASSERT_TRUE(outsideOrbit.has_value()) << imagePath.string();

		Vision vision{Source::Image};
		vision.setSetupImage(imagePath);

		for (const Coord gaugeCoord: orbit) {
			EXPECT_TRUE(vision.setup(gaugeCoord)) << imagePath.string() << " gauge=(" << gaugeCoord.x << "," << gaugeCoord.y << ")";
		}
		EXPECT_FALSE(vision.setup(*outsideOrbit)) << imagePath.string() << " invalid=(" << outsideOrbit->x << "," << outsideOrbit->y << ")";
	}
}

} // namespace gtest
} // namespace tengen::vision
