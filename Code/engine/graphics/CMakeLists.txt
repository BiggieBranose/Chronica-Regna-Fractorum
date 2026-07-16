# ============================================================================
# graphics/CMakeLists.txt — Build configuration for the graphics module
#
# This file defines the crf_graphics static library. It contains:
#   - Window.cpp (GLFW window management + Vulkan surface creation)
#
# Dependencies:
#   - crf_core  — for Log, Assert, Types, Platform
#   - glfw      — for window creation and input
#   - Vulkan    — for VkInstance, VkSurfaceKHR types
#
# How it works:
#   find_package(glfw3 REQUIRED) tells CMake to locate the GLFW library.
#   On MSYS2/MinGW, GLFW is installed in /mingw64/lib/libglfw3.a and
#   CMake finds it automatically if the PATH is set correctly.
#
#   target_link_libraries(crf_graphics PUBLIC crf_core glfw Vulkan::Vulkan)
#   means: "crf_graphics depends on crf_core, glfw, and Vulkan."
#   PUBLIC means any target that links crf_graphics also gets these
#   dependencies transitively. For example, crf_game links crf_graphics,
#   so it automatically gets crf_core, glfw, and Vulkan too.
# ============================================================================

find_package(glfw3 REQUIRED)

add_library(crf_graphics STATIC
    Window.cpp
)

target_link_libraries(crf_graphics PUBLIC crf_core glfw Vulkan::Vulkan)
target_include_directories(crf_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
