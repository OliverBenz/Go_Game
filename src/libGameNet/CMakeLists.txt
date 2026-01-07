# Settings
set(targetName libGameNet)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/include/gameNet/nwEvents.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/gameNet/types.hpp
    ${CMAKE_CURRENT_LIST_DIR}/include/gameNet/sessionManager.hpp
)

set(sources
    ${CMAKE_CURRENT_LIST_DIR}/nwEvents.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sessionManager.cpp
)


add_library(${targetName} STATIC
    ${headers} ${sources}
)
add_library(go::gameNet ALIAS ${targetName})

target_include_directories(${targetName}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(${targetName} PRIVATE
    go::network
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
