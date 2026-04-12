include_guard(GLOBAL)

set(targetName "visionCore.gtest")

add_executable(${targetName}
	"${CMAKE_CURRENT_LIST_DIR}/syntheticBoard.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/rectifier.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/process.gtest.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/boardFinder.gtest.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/stoneFinder.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::vision::core
        GTest::gtest_main
)
set_target_properties(${targetName}
    PROPERTIES
        FOLDER "${ideFolderSource}"
)

# Setup project settings
set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

# Add tests
include(GoogleTest)
gtest_discover_tests(${targetName})
