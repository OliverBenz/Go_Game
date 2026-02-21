# Foundations (reusable across domains)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/core")   # Library: Board image detection algorithm
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/core")      # Library: General network layer 

# Game Domain
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/model")    # Library: Core data layer
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/core")     # Library: Core game logic

# Adapters / Integration
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/network")   # Library: Go game networking
 # add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/perception") # Library: Image detection on game model

# Runtime + Apps
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/apps/runtime")   # Library:     Runtime/Orchestration
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/apps/tengen")    # Application: The actual game (GUI wrapper of runtime)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/apps/server/")   # Application: Server

