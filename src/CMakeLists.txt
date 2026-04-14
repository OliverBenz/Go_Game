# Foundations (reusable across domains)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/core")       # Library: Board image detection algorithm
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/core")          # Library: General network layer

# Game Domain
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/model")        # Library: Core data layer
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/core")         # Library: Core game logic

# Adapters / Integration
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/net/network")       # Library: Go game networking
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/perception") # Library: Image detection on game model

# Runtime
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/runtime")      # Library: Core business logic of applications
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/game/gui")          # Library: GUI components for the applications

# Applications
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/apps/tengen")       # Application: The actual game (GUI wrapper of runtime)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/apps/server/")      # Application: Server


# Dev Tools
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vision/devtools")   # Library: 
