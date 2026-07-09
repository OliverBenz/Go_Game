include_guard(GLOBAL)

set(targetName visionTuner)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED)

set(headers
	"${CMAKE_CURRENT_LIST_DIR}/pipelineStep.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/analyser.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/mainWindow.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/webcam.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/timer.hpp"
)
set(sources
	"${CMAKE_CURRENT_LIST_DIR}/main.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/analyser.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/mainWindow.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/webcam.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/timer.cpp"
)

add_executable(${targetName} ${headers} ${sources})

target_link_libraries(${targetName}
	PRIVATE
		tengen::vision::core
		Qt6::Widgets
)
target_compile_definitions(${targetName}
	PRIVATE
		PATH_TEST_IMG="${CMAKE_SOURCE_DIR}/tests/vision/resources/"
)
set_target_properties(${targetName}
	PROPERTIES
		AUTOMOC ON
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})
