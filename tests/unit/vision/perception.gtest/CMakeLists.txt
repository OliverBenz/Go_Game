include_guard(GLOBAL)

set(targetName "visionPerception.unit.gtest")

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/symmetryBreaking.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::vision::core
        tengen::vision::perception
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
