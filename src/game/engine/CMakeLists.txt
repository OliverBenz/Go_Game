include_guard(GLOBAL)

set(targetName gameEngine)

set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/engine/types.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/engine/IEngineSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/engine/gtp.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/engine/katagoSession.hpp"
)

set(sources
    "${CMAKE_CURRENT_LIST_DIR}/gtp.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/katagoSession.cpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::game::engine ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(${targetName}
    PUBLIC
        tengen::game::model
)

set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
