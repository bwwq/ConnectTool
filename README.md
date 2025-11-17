# ConnectTool - Dear ImGui Hello World

This repository includes a simple Dear ImGui "Hello World" example using GLFW + OpenGL3 for cross-platform compatibility.

## Prerequisites

- C++17 compatible compiler
- CMake 3.10 or higher
- OpenGL 3.0 or higher
- GLFW library

## Getting Dear ImGui

1. Download Dear ImGui from https://github.com/ocornut/imgui
2. Extract the contents to a folder named `imgui` in the project root (next to `CMakeLists.txt`)

## Building

1. Ensure you have GLFW installed. On Windows, you can use vcpkg:
   ```
   vcpkg install glfw3
   ```
   On Linux/macOS, use your package manager (e.g., `sudo apt install libglfw3-dev` on Ubuntu).

2. Create a build directory:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Run the executable:
   ```
   ./ConnectTool
   ```

## Cross-Platform Notes

- This setup uses GLFW for window management and OpenGL for rendering, ensuring compatibility across Windows, Linux, and macOS.
- If using Visual Studio on Windows, you can generate VS solution files with `cmake -G "Visual Studio 16 2019" ..` (adjust for your version).
- For macOS, ensure you have Xcode command line tools installed.
