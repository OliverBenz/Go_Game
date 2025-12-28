cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
  asio_upstream
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-36-0   # lock exact version (example tag)
)
FetchContent_MakeAvailable(asio_upstream)

add_library(asio INTERFACE)
target_include_directories(asio
  SYSTEM INTERFACE
    ${asio_upstream_SOURCE_DIR}/asio/include
)
target_compile_definitions(asio
  INTERFACE
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

find_package(Threads REQUIRED)
target_link_libraries(asio INTERFACE Threads::Threads)

# Optional: on Windows you usually need ws2_32
if (WIN32)
  target_link_libraries(asio INTERFACE ws2_32)
endif()