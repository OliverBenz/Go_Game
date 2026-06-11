include_guard(GLOBAL)

set(targetName gameEngine)

# Get files to build
set(headers
				"${CMAKE_CURRENT_LIST_DIR}/include/core/serializer.hpp"
)
set(sources
				"${CMAKE_CURRENT_LIST_DIR}/serializer.cpp"
)

# Create target
add_library(${targetName} STATIC ${headers} ${sources})
add_library(tengen::game::engine ALIAS ${targetName})

target_include_directories(${targetName}
				PUBLIC
								"${CMAKE_CURRENT_LIST_DIR}/include"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})  # Set the output directory of the library
