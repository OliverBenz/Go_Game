# Setup project settings
include(cmake/ProjectSettings.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/CustomCommands.cmake)

# Add libraries
include(gtest.cmake)
include(logger.cmake)
include(asio.cmake)
include(nlohmann_json.cmake)
