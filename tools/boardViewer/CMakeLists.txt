include_guard(GLOBAL)

set(targetName boardViewer)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(${targetName} "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

target_link_libraries(${targetName}
	PRIVATE
		tengen::game::core
		tengen::game::gui
		tengen::vision::devtools
		Qt6::Widgets
)
target_compile_definitions(${targetName}
	PRIVATE
		PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/vision/resources/"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
