cmake_minimum_required(VERSION 3.12)

project(Go_Game)
set(CMAKE_CXX_STANDARD 20)

option(BUILD_TESTS "Create the unit tests for the project." False)
option(BUILD_EXAMPLES "Create examples for the project." False)


# Add libraries to project
add_subdirectory(lib)

# Add source, tests and examples
add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
