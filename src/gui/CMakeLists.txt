# Settings
set(targetName goUi)

# External libraries
find_package(glfw3 REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

add_executable(${targetName}
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gameWindow.cpp
    ${CMAKE_CURRENT_LIST_DIR}/boardRenderer.cpp
)

target_link_libraries(${targetName} PRIVATE
    goCore
    glfw
    SDL2::SDL2
    SDL2_image::SDL2_image
)
target_include_directories(${targetName} PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)
target_compile_definitions(${targetName} PRIVATE TEXTURE_BLACK="${CMAKE_SOURCE_DIR}/assets/anime_black.png")
target_compile_definitions(${targetName} PRIVATE TEXTURE_WHITE="${CMAKE_SOURCE_DIR}/assets/anime_white.png")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

