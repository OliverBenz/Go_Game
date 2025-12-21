# Settings
set(targetName goUi)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(${targetName}
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/boardRenderer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/MainWindow.cpp
    ${CMAKE_CURRENT_LIST_DIR}/BoardWidget.cpp
)

target_link_libraries(${targetName} PRIVATE
    goCore
    Qt6::Widgets
)
set_target_properties(${targetName} PROPERTIES AUTOMOC ON) # Auto handling of QT Meta-Object Compiler
target_compile_definitions(${targetName} PRIVATE TEXTURE_BLACK="${CMAKE_SOURCE_DIR}/assets/anime_black.png")
target_compile_definitions(${targetName} PRIVATE TEXTURE_WHITE="${CMAKE_SOURCE_DIR}/assets/anime_white.png")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)
