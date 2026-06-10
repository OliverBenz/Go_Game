include_guard(GLOBAL)

set(targetName gameRuntime.integration)

set(kataGoRoot "${CMAKE_SOURCE_DIR}/../KataGo/cpp")
set(kataGoExecutable "${kataGoRoot}/katago")
set(kataGoConfig "${kataGoRoot}/configs/gtp_example.cfg")

file(GLOB kataGoModels
    "${kataGoRoot}/*.bin.gz"
    "${kataGoRoot}/*.txt.gz"
)

set(kataGoModel "")
foreach(modelPath ${kataGoModels})
    if(NOT modelPath MATCHES "human")
        set(kataGoModel "${modelPath}")
        break()
    endif()
endforeach()

add_executable(${targetName}
    "${CMAKE_CURRENT_LIST_DIR}/botSession.gtest.cpp"
)

target_link_libraries(${targetName}
    PRIVATE
        tengen::game::runtime
        GTest::gtest_main
)

target_compile_definitions(${targetName}
    PRIVATE
        TEST_KATAGO_EXECUTABLE="${kataGoExecutable}"
        TEST_KATAGO_CONFIG="${kataGoConfig}"
        TEST_KATAGO_MODEL="${kataGoModel}"
)

set_target_properties(${targetName}
    PROPERTIES
        FOLDER "${ideFolderSource}"
)

set_target_configuration(${targetName} Config::Default)
set_output_directory(${targetName})

include(GoogleTest)
gtest_discover_tests(${targetName}
    WORKING_DIRECTORY "/tmp"
    PROPERTIES
        LABELS "integration;katago"
        RUN_SERIAL TRUE
)
