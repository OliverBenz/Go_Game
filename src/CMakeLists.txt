add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libCore/)     # Library:     Core game logic
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libNetwork/)  # Library:     Network layer networking
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libGameNet/)  # Library:     Application layer networking
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libCamera/)   # Library:     Physical board detection

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/gui/)         # Application: GUI
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/server/)      # Application: Server