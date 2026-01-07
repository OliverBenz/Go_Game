# Settings
set(targetName libGameNet)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/gameNet/protocol.hpp
)

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/protocol.cpp
)


add_library(${targetName} STATIC
    ${headers} ${sources}
)
add_library(go::gameNet ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
