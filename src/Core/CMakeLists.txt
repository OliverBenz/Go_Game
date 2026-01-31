set(targetName Core)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/core/game.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/position.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/gameEvent.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/IGameStateListener.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/IGameSignalListener.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/SafeQueue.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/sgfHandler.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/moveChecker.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/core/eventHub.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/zobristHash.hpp"
)
set(sources
    "${CMAKE_CURRENT_LIST_DIR}/position.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/game.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sgfHandler.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/moveChecker.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/eventHub.cpp"
)

# Create target
add_library(${targetName} STATIC ${headers} ${sources})
add_library(go::core ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_link_libraries(${targetName} 
    PUBLIC
        go::data    
)

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)
