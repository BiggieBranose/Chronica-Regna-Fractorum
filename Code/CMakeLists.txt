cmake_minimum_required(VERSION 3.16)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan)
find_package(glfw3)
#find_package(glm)

add_executable(VulkanTest src/main.cpp)

set(Vulkan_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(Vulkan_LIBRARY "${CMAKE_SOURCE_DIR}/libraries/vulkan-1.lib")

target_link_libraries(VulkanTest
    PRIVATE
        ${Vulkan_LIBRARY}
        glfw
    Vulkan::Vulkan
)

target_include_directories(VulkanTest
    PRIVATE
        ${Vulkan_INCLUDE_DIR}
)
include_directories(VulkanTest
    ${Vulkan_INCLUDE_DIR}
)