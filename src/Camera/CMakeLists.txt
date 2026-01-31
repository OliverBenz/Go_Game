set(targetName Camera)

find_package(OpenCV REQUIRED)

# Get files to build
set(headers )
set(sources "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

# TODO: This core/ will be a library. Not the executable.
# Create target
add_executable(${targetName} ${headers}${sources})

target_include_directories(${targetName} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${targetName} PRIVATE ${OpenCV_LIBS})

# TODO: Move to tests once the library stands
target_compile_definitions(${targetName} PRIVATE PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/camera/resources/")

# Setup project settings
# set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)
