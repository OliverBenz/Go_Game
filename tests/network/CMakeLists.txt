# Settings
set(targetName "network.gtest")

# Create executable
add_executable(${targetName}
    ${CMAKE_CURRENT_LIST_DIR}/server.gtest.cpp
)

# Link to required libraries
target_link_libraries(${targetName} PUBLIC go::network go::gameNet GTest::gtest_main)
set_target_properties(${targetName} PROPERTIES FOLDER "${ideFolderSource}")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Add tests
add_test(NAME ${targetName} COMMAND $<TARGET_FILE:${targetName}>)
