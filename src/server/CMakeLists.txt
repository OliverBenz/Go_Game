# Settings
set(targetName goServer)

add_executable(${targetName}
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp    
)

target_link_libraries(${targetName}
    PRIVATE
        go::app
)

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})
