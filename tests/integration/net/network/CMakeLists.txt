include_guard(GLOBAL)

set(targetName "netNetwork.gtest")

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/mockClient.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mockServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/basic.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nwEvents.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::net::gameNet
        GTest::gtest_main
        nlohmann_json::nlohmann_json
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
