add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/core.unit")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/core.integration")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/network.unit")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/network.integration")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/core.unit")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/core.integration")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/perception.integration")
