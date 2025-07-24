# Settings
set(targetName Template)

# Get files to build
set(headers )
set(sources ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

# Create target
add_library(${targetName} STATIC
    ${headers}
    ${sources}
)


# Link to thread module
# target_link_libraries(${targetName} PRIVATE fmt)
# target_include_directories(${targetName} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# set_target_properties(${targetName} PROPERTIES FOLDER "${ideFolderSource}")

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Copy header files to output after build
# copy_headers_to_output(${targetName} "${headers}" "")

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

# System installation
# install(TARGETS ${targetName} DESTINATION lib)
# install(FILES ${headers} DESTINATION include/${targetName})