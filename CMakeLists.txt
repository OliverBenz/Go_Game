cmake_minimum_required(VERSION 3.14)

project(Go_Game VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Create the unit tests for the project." ON)

# Add libraries to project
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/lib")

# Add source, tests and examples
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/src")
if(BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
