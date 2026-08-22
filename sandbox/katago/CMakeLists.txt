include_guard(GLOBAL)

set(targetName sbKatago)
add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)

target_compile_definitions(${targetName} PRIVATE KATAGO_PATH="${CMAKE_CURRENT_LIST_DIR}/katago_final")

# Setup project settings
# set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
