cmake_minimum_required(VERSION 3.16)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(VulkanTest src/main.cpp)

# Platform-specific Vulkan handling
if (WIN32)
    message(STATUS "Using local Vulkan library (Windows)")

    target_include_directories(VulkanTest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(VulkanTest PRIVATE
        ${CMAKE_SOURCE_DIR}/libraries/vulkan-1.lib
        glfw
    )
else()
    message(STATUS "Using system Vulkan (Linux/macOS)")

    target_link_libraries(VulkanTest PRIVATE
        Vulkan::Vulkan
        glfw
    )
endif()