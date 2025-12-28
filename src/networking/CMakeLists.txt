# Settings
set(targetName Networking)

# External libraries
# TODO: Have this stuff under lib and use latest asio version
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/lib/asio/include")
target_link_libraries(asio INTERFACE pthread)
target_compile_definitions(asio
    PRIVATE
        ASIO_STANDALONE
        ASIO_NO_DEPRECATED
)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/networking/protocol.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/networking/server.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/networking/client.hpp
)

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/client.cpp
)


add_library(${targetName} STATIC
    ${headers} ${sources}
)

target_include_directories(${targetName}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# TODO: Implement PIMPLs to hide asio. Make this a private linking
target_link_libraries(${targetName}
    PUBLIC
        asio
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
