include_guard(GLOBAL)

set(targetName gameRuntime.unit)

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/botSession.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::game::runtime
        GTest::gtest_main
)

set_target_properties(${targetName}
    PROPERTIES
        FOLDER "${ideFolderSource}"
)

set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

include(GoogleTest)
gtest_discover_tests(${targetName})
