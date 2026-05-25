include_guard(GLOBAL)

set(targetName tengen)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MainWindow.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/MainWindowPresenter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/GamePresenter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/BoardPresenter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ChatPresenter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ConnectDialog.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/HostDialog.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/RulesDialog.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Logging.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        Logger::Logger
        tengen::game::runtime
        tengen::game::gui
        Qt6::Widgets
)

set_target_properties(${targetName}
    PROPERTIES
        AUTOMOC ON # Auto handling of QT Meta-Object Compiler
)

# Copy help files to output
add_custom_command(
    TARGET ${targetName}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${targetName}>/help"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/help/rules.html"
            "$<TARGET_FILE_DIR:${targetName}>/help/rules.html"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
