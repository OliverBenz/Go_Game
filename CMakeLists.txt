cmake_minimum_required(VERSION 3.14)

project(Tengen
	VERSION 0.3.0
	LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TENGEN_BUILD_TESTS "Create the unit tests for the project." ON)
option(TENGEN_BUILD_TOOLS "Create the tools for the project." ON)

# Add libraries to project
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/lib")

# Add source, tests, and tools
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/src")
if(TENGEN_BUILD_TOOLS)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tools")
endif()

if(TENGEN_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
