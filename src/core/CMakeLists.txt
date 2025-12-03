# Settings
set(targetName goCore)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/core/types.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/core/board.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/core/game.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/core/gameEvent.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/core/SafeQueue.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/core/sgfHandler.hpp
)
set(sources
    ${CMAKE_CURRENT_LIST_DIR}/board.cpp
    ${CMAKE_CURRENT_LIST_DIR}/game.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sgfHandler.cpp
)

# Create target
add_library(${targetName}
    ${headers}
    ${sources}
)


# Setup project settings
# set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

