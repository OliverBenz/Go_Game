include_guard(GLOBAL)

set(targetName testing)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED)

add_executable(${targetName} "${CMAKE_CURRENT_LIST_DIR}/main.cpp")

target_include_directories(${targetName}
PRIVATE
${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(${targetName}
	PRIVATE
		Qt6::Widgets
		${OpenCV_LIBS}
)
target_compile_definitions(${targetName}
	PRIVATE
		PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/vision/resources/"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
