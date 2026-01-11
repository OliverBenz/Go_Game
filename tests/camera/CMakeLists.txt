# Settings
set(targetName "camera.gtest")

find_package(OpenCV REQUIRED)

# Create executable
add_executable(${targetName}
    ${CMAKE_CURRENT_LIST_DIR}/boardDetect.gtest.cpp
    ${CMAKE_SOURCE_DIR}/src/libCamera/BoardDetect.cpp
)

# Link to required libraries
target_link_libraries(${targetName} PUBLIC ${OpenCV_LIBS} GTest::gtest_main)
set_target_properties(${targetName} PROPERTIES FOLDER "${ideFolderSource}")

target_include_directories(${targetName} PRIVATE ${CMAKE_SOURCE_DIR}/src/libCamera ${OpenCV_INCLUDE_DIRS})

# Setup project settings
set_project_warnings(${targetName})
set_compile_options(${targetName})
set_output_directory(${targetName})

target_compile_definitions(${targetName} PRIVATE PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/camera/resources/")

# Add tests
add_test(NAME ${targetName} COMMAND $<TARGET_FILE:${targetName}>)
