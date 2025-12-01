# Settings
set(targetName goUi)

# External libraries
find_package(glfw3 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Get files to build
set(headers)
set(sources
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/game.cpp
    ${CMAKE_CURRENT_LIST_DIR}/board.cpp
    ${CMAKE_CURRENT_LIST_DIR}/window.cpp
)

# TODO: Make this a library included in the game.
# Create target
add_executable(${targetName}
    ${headers}
    ${sources}
)

target_link_libraries(${targetName} PRIVATE
    glfw
    SDL2::SDL2
    SDL2_image::SDL2_image
    Qt6::Widgets
)
target_include_directories(${targetName} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

set_target_properties(${targetName} PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

