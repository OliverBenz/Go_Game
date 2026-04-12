include_guard(GLOBAL)

set(targetName "gameCore.gtest")

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/game.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/moveChecker.gtest.cpp"
)

# Link to required libraries
target_link_libraries(${targetName}
    PRIVATE
        tengen::game::core
        GTest::gtest_main
)
set_target_properties(${targetName}
    PROPERTIES
        FOLDER "${ideFolderSource}"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

# Add tests
include(GoogleTest)
gtest_discover_tests(${targetName})
