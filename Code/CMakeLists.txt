cmake_minimum_required(VERSION 3.16)
project(Chronica_Regna_Fractorum_Test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

# Ensure main.cpp exists before creating target
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(Chronica_Regna_Fractorum_Test src/main.cpp)
    target_link_libraries(Chronica_Regna_Fractorum_Test PRIVATE Vulkan::Vulkan)
else()
    message(FATAL_ERROR "src/main.cpp not found!")
endif()