cmake_minimum_required(VERSION 3.10)
project(ConnectTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Boost REQUIRED)

# Definitions
add_definitions(-D_WIN32_WINNT=0x0601)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/steamworks/public)
include_directories(${CMAKE_SOURCE_DIR}/steamworks/public/steam)
include_directories(${CMAKE_SOURCE_DIR}/steam_sdk/public/steam)
include_directories(${CMAKE_SOURCE_DIR}/nanoid_root)
include_directories(${CMAKE_SOURCE_DIR}/net)

# Source files
file(GLOB SOURCES
    "online_game_tool.cpp"
    "net/*.cpp"
    "steam/*.cpp"
)

# Create executable
add_executable(ConnectTool ${SOURCES})

# Link libraries
# Link libraries
target_link_libraries(ConnectTool
    Boost::headers
    ws2_32
    ${CMAKE_SOURCE_DIR}/steam_sdk/lib/steam_api64.lib
)


