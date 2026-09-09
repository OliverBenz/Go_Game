include_guard(GLOBAL)

set(targetName visionCore)

find_package(OpenCV REQUIRED COMPONENTS
	core
	imgproc
	imgcodecs
	videoio
	highgui
	geometry
)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/statistics.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/gridLatticeFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/boardFinderInternal.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/core/debugVisualizer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/core/boardFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/core/gridFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/core/stoneFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/core/stoneFinderConfig.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/decisionPolicy.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/debugging.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/featureExtraction.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/features.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/geometrySampling.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/modelCalibration.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/refinementEngine.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/scoring.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/stoneFinderInternal.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/stoneFinderTypes.hpp"
)
set(sources
    "${CMAKE_CURRENT_LIST_DIR}/statistics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/debugVisualizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/boardFinderInternal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/boardFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gridLatticeFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gridFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/debugging.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/decisionPolicy.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/featureExtraction.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/geometrySampling.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/modelCalibration.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/refinementEngine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/scoring.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/stoneFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder/stoneFinderInternal.cpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::vision::core ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
		${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${targetName}
	PUBLIC
		${OpenCV_LIBS}
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

target_compile_definitions(${targetName}
	PRIVATE
		$<$<CONFIG:Debug>:VISION_DEBUG_LOGGING>
		# VISION_LOG_BOARDFINDER
		# VISION_LOG_GRIDFINDER
		# VISION_LOG_STONEFINDER
)
