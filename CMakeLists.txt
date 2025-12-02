cmake_minimum_required(VERSION 3.10)
project(ConnectTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Boost REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/imgui)
include_directories(${CMAKE_SOURCE_DIR}/imgui/backends)
include_directories(${CMAKE_SOURCE_DIR}/steamworks/public)
include_directories(${CMAKE_SOURCE_DIR}/steamworks/public/steam)
include_directories(${CMAKE_SOURCE_DIR}/net)
include_directories(${CMAKE_SOURCE_DIR}/ui)
include_directories(${CMAKE_SOURCE_DIR}/config)

# Source files
file(GLOB SOURCES
    "online_game_tool.cpp"
    "imgui/*.cpp"
    "imgui/backends/imgui_impl_glfw.cpp"
    "imgui/backends/imgui_impl_opengl3.cpp"
    "net/*.cpp"
    "steam/*.cpp"
    "config/*.cpp"
)

# Create executable
add_executable(ConnectTool ${SOURCES})

# Link libraries
if(WIN32)
    target_link_libraries(ConnectTool
        glfw
        OpenGL::GL
        Boost::headers
        ${CMAKE_SOURCE_DIR}/steamworks/redistributable_bin/win64/steam_api64.lib
        ws2_32
    )
    
    # Copy steam_api64.dll to output directory
    add_custom_command(TARGET ConnectTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/steamworks/redistributable_bin/win64/steam_api64.dll
        $<TARGET_FILE_DIR:ConnectTool>/steam_api64.dll
    )
elseif(APPLE)
    target_link_libraries(ConnectTool
        glfw
        OpenGL::GL
        Boost::headers
        ${CMAKE_SOURCE_DIR}/steamworks/redistributable_bin/osx/libsteam_api.dylib
    )
    
    # Copy libsteam_api.dylib
    add_custom_command(TARGET ConnectTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/steamworks/redistributable_bin/osx/libsteam_api.dylib
        $<TARGET_FILE_DIR:ConnectTool>/libsteam_api.dylib
    )
else()
    # Linux
    target_link_libraries(ConnectTool
        glfw
        OpenGL::GL
        Boost::headers
        ${CMAKE_SOURCE_DIR}/steamworks/redistributable_bin/linux64/libsteam_api.so
    )
endif()
