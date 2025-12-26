cmake_minimum_required(VERSION 3.14)

include(FetchContent)

# GTest
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # For Windows: Prevent overriding the parent project's compiler/linker settings

FetchContent_MakeAvailable(googletest)
set_target_properties(gtest      PROPERTIES FOLDER "${IDE_FOLDER_EXTERNAL}")
set_target_properties(gtest_main PROPERTIES FOLDER "${IDE_FOLDER_EXTERNAL}")
set_target_properties(gmock      PROPERTIES FOLDER "${IDE_FOLDER_EXTERNAL}")
set_target_properties(gmock_main PROPERTIES FOLDER "${IDE_FOLDER_EXTERNAL}")

# Logger
FetchContent_Declare(
  Logger
  GIT_REPOSITORY https://github.com/OliverBenz/Logger.git
  GIT_TAG        990f70445da5ddbf74f8af97b9f5a7773a9f10dd
)
set(LOGGER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Logger)
