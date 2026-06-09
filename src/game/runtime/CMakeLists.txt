include_guard(GLOBAL)

set(targetName gameRuntime)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/botSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/networkSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/openSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/IAppSignal.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/IGameSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/IChatSession.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/eventHub.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/position.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/tengen/gameServer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/logging.hpp"
)

set(sources
    "${CMAKE_CURRENT_LIST_DIR}/botSession.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/networkSession.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/openSession.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/eventHub.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/position.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/gameServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/logging.cpp"
)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::game::runtime ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

# TODO: GameNet should be private.
target_link_libraries(${targetName}
    PRIVATE
        Logger::Logger
    PUBLIC
		tengen::game::engine
		tengen::game::core
        tengen::net::gameNet
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
