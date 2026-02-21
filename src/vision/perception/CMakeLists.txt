set(targetName visionPerception)

# TODO: We still need this if go::vision::core links OpenCV privately?
find_package(OpenCV REQUIRED)

# Get files to build
set(headers)
set(sources)

add_library(${targetName} STATIC ${headers} ${sources})
add_library(go::vision::perception ALIAS ${targetName})

target_include_directories(${targetName}
	PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/include"
)
target_link_libraries(${targetName}
	PRIVATE go::vision::core
)

# Setup project settings
set_project_warnings(${targetName})  # Which warnings to enable
set_compile_options(${targetName})   # Which extra compiler flags to enable
set_output_directory(${targetName})  # Set the output directory of the library

# Specify version
target_compile_definitions(${targetName} PUBLIC VERSION_MAJOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_MINOR=0)
target_compile_definitions(${targetName} PUBLIC VERSION_PATCH=1)

