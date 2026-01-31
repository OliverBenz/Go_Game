add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Data/")     # Library:     Core data layer
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Core/")     # Library:     Core game logic
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Network/")  # Library:     Network layer networking
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/GameNet/")  # Library:     Application layer networking
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Camera/")   # Library:     Physical board detection
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/App/")      # Library:     Application logic

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/gui/")         # Application: GUI
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/server/")      # Application: Server