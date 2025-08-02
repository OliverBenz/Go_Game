# Settings
# set(targetName goNetworking)


# External libraries
# TODO: Have this stuff under lib and use latest asio version
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE "${CMAKE_SOURCE_DIR}/lib/asio/include")
target_link_libraries(asio INTERFACE pthread)


# TODO: This should be in a library

add_executable(goNetClient ${CMAKE_CURRENT_LIST_DIR}/client.cpp)
# set_project_warnings(${targetName})  # Which warnings to enable
target_link_libraries(goNetClient PRIVATE asio)
set_compile_options(goNetClient)   # Which extra compiler flags to enable
set_output_directory(goNetClient)  # Set the output directory of the library

add_executable(goNetServer ${CMAKE_CURRENT_LIST_DIR}/server.cpp)
# set_project_warnings(${targetName})  # Which warnings to enable
target_link_libraries(goNetServer PRIVATE asio)
set_compile_options(goNetServer)   # Which extra compiler flags to enable
set_output_directory(goNetServer)  # Set the output directory of the library


# Specify version
# target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
# target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
# target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)
