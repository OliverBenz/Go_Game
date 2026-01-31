# Settings
set(targetName "gameNet.gtest")

# Create executable
add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/mockClient.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/mockServer.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/basic.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/nwEvents.gtest.cpp"
)

# Link to required libraries
target_link_libraries(${targetName}
    PRIVATE
        go::gameNet
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)
set_target_properties(${targetName} PROPERTIES FOLDER "${ideFolderSource}")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Add tests
include(GoogleTest)
gtest_discover_tests(${targetName})
