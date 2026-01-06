# Settings
set(targetName goServer)

# Get files to build
set(headers
    ${CMAKE_CURRENT_LIST_DIR}/gameServer.hpp
    ${CMAKE_CURRENT_LIST_DIR}/sessionManager.hpp
    ${CMAKE_CURRENT_LIST_DIR}/Logging.hpp
)
set(sources
    ${CMAKE_CURRENT_LIST_DIR}/gameServer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sessionManager.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Logging.cpp
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
)

add_executable(${targetName}
    ${headers} ${sources}
)

target_link_libraries(${targetName} PRIVATE
    go::core go::network Logger::Logger
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
