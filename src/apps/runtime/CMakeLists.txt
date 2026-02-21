set(targetName appRuntime)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/sessionManager.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/IAppSignalListener.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/eventHub.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/position.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/gameServer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/logging.hpp"
)

set(sources
    "${CMAKE_CURRENT_LIST_DIR}/sessionManager.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/eventHub.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/position.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gameServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/logging.cpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::runtime ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

# TODO: GameNet should be private.
target_link_libraries(${targetName}
    PRIVATE
        Logger::Logger
    PUBLIC
		tengen::game::core tengen::net::gameNet
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
