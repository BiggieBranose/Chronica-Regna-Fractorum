cmake_minimum_required(VERSION 3.16)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(VulkanTest src/main.cpp)

target_link_libraries(VulkanTest
    Vulkan::Vulkan
    submodules/glfw
    glm
)

target_include_directories(VulkanTest
    include
)

add_subdirectory(
    submodules/glfw
)