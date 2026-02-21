set(targetName netNetwork)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/network/client.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/network/server.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/network/nwEvents.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/network/types.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/sessionManager.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/serverEvents.hpp"
)

set(sources
    "${CMAKE_CURRENT_LIST_DIR}/client.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/server.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nwEvents.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/sessionManager.cpp"
)


add_library(${targetName} STATIC ${headers} ${sources})
add_library(go::net::gameNet ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(${targetName}
    PUBLIC
        go::game::model
    PRIVATE
        go::net::core
        nlohmann_json::nlohmann_json
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
