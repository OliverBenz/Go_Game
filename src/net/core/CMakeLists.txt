set(targetName netCore)

# Get files to build
set(headers
    "${CMAKE_CURRENT_LIST_DIR}/include/network/protocol.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/network/tcpServer.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/network/tcpClient.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/connection.hpp"
)

set(sources
    "${CMAKE_CURRENT_LIST_DIR}/tcpServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/tcpClient.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/connection.cpp"
)


add_library(${targetName} STATIC ${headers} ${sources})
add_library(go::net::core ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/include"
)

target_link_libraries(${targetName}
    PRIVATE
        asio
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
