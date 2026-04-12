include_guard(GLOBAL)

set(targetName "netCore.gtest")

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/server.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::net::core
        tengen::net::gameNet
        GTest::gtest_main
)
set_target_properties(${targetName} PROPERTIES
    FOLDER "${ideFolderSource}"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

# Add tests
include(GoogleTest)
gtest_discover_tests(${targetName})
