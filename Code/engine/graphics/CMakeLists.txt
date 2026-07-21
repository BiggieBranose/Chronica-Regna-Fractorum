find_package(glfw3 REQUIRED)

add_library(crf_graphics STATIC
    Window.cpp
    Vertex.cpp
    VulkanContext.cpp
    VulkanRenderPass.cpp
    VulkanPipeline.cpp
    VulkanBuffer.cpp
    VulkanTexture.cpp
    VulkanDescriptor.cpp
    ModelLoader.cpp
    AccelerationStructure.cpp
    RaytracingPipeline.cpp
    ../../external/stb_image_impl.cpp
)

target_link_libraries(crf_graphics PUBLIC crf_core glfw Vulkan::Vulkan)
target_include_directories(crf_graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(crf_graphics PRIVATE -isystem "${CMAKE_CURRENT_SOURCE_DIR}/../../external")
else()
    target_include_directories(crf_graphics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../external)
endif()

if (CRF_WARNINGS_AS_ERRORS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crf_graphics PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(crf_graphics PRIVATE /W4 /WX)
    endif()
endif()
