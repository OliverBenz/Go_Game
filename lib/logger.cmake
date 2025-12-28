cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
  Logger
  GIT_REPOSITORY https://github.com/OliverBenz/Logger.git
  GIT_TAG        955d8c4f97b837af25c5da4ae28cb8ef1170e2ce
)
set(LOGGER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Logger)
