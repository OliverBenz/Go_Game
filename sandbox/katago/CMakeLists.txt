include_guard(GLOBAL)

set(targetName sbKatago)
add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
