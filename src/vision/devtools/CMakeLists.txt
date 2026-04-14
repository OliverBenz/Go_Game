include_guard(GLOBAL)

set(targetName visionDevtools)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/vision/devtools/syntheticBoard.hpp"
)
set(sources
    "${CMAKE_CURRENT_LIST_DIR}/syntheticBoard.cpp"
)

# Create target
add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::vision::devtools ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_link_libraries(${targetName}
    PUBLIC
        tengen::vision::core
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})  # Set the output directory of the library
