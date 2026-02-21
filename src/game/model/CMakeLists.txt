set(targetName gameModel)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/model/player.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/model/coordinate.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/model/board.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/model/gameStatus.hpp"
)
set(sources
    "${CMAKE_CURRENT_LIST_DIR}/board.cpp"
)

# Create target
add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::game::model ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library
