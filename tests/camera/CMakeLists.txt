# Settings
set(targetName "camera.gtest")

# Create executable
add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/rectifier.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/process.gtest.cpp"
)

# Link to required libraries
target_link_libraries(${targetName} PRIVATE go::camera GTest::gtest_main)
set_target_properties(${targetName} PROPERTIES FOLDER "${ideFolderSource}")

target_compile_definitions(${targetName} PRIVATE PATH_TEST_IMG="${CMAKE_CURRENT_LIST_DIR}/resources/")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Add tests
include(GoogleTest)
gtest_discover_tests(${targetName})