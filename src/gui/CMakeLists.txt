# Settings
set(targetName goUi)

# External libraries
find_package(glfw3 REQUIRED)

# Get files to build
set(headers ${CMAKE_CURRENT_LIST_DIR}/window.hpp)
set(sources ${CMAKE_CURRENT_LIST_DIR}/window.cpp)

# Create target
add_library(${targetName} STATIC
    ${headers}
    ${sources}
)

target_link_libraries(${targetName} PRIVATE
    glfw
)

# Setup project settings
# set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

