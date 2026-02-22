set(targetName visionPerception)

# TODO: We still need this if tengen::vision::core links OpenCV privately?
find_package(OpenCV REQUIRED)

# Get files to build
set(headers
	"${CMAKE_CURRENT_LIST_DIR}/include/vision/source.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/include/vision/vision.hpp"
)
set(sources
	"${CMAKE_CURRENT_LIST_DIR}/vision.hpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::vision::perception ALIAS ${targetName})

target_include_directories(${targetName}
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_link_libraries(${targetName}
	PRIVATE
		tengen::vision::core
	PUBLIC
		tengen::game::model
)

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library
