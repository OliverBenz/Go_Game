set(targetName Camera)

find_package(OpenCV REQUIRED)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/statistics.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/camera/debugVisualizer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/camera/boardFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/gridFinder.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/camera/rectifier.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/camera/stoneFinder.hpp"
)
set(sources
    "${CMAKE_CURRENT_LIST_DIR}/statistics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/debugVisualizer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/boardFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gridFinder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/rectifier.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder.cpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(go::camera ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
		${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${targetName}
	PUBLIC ${OpenCV_LIBS}
)

# Setup project settings
# set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=1)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=0)

