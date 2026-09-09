include_guard(GLOBAL)

set(targetName boardViewer)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED)

set(headers
	"${CMAKE_CURRENT_LIST_DIR}/mainWindow.hpp"
)
set(sources
	"${CMAKE_CURRENT_LIST_DIR}/main.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/mainWindow.cpp"
)

add_executable(${targetName} ${headers} ${sources})

target_link_libraries(${targetName}
	PRIVATE
		tengen::game::core
		tengen::game::gui
		tengen::vision::core
		nlohmann_json::nlohmann_json
		Qt6::Widgets
)
set_target_properties(${targetName}
	PROPERTIES
		AUTOMOC ON
)
target_compile_definitions(${targetName}
	PRIVATE
		PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/vision/resources/"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
