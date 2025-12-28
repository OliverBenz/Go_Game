# Settings
set(targetName libNetwork)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/network/protocol.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/network/server.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/network/client.hpp
    ${CMAKE_CURRENT_LIST_DIR}/Logging.hpp
)

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/client.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Logging.cpp
)


add_library(${targetName} STATIC
    ${headers} ${sources}
)
add_library(go::network ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# TODO: Implement PIMPLs to hide asio. Make this a private linking
target_link_libraries(${targetName}
    PUBLIC
        asio
    PRIVATE
        Logger::Logger
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
