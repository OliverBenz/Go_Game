cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
  Logger
  GIT_REPOSITORY https://github.com/OliverBenz/Logger.git
  GIT_TAG        8e8f8e4a61c13ae370b5295775f92b14920bbc1a
)
set(LOGGER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Logger)
