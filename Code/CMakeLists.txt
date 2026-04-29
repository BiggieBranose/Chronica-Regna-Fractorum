cmake_minimum_required(VERSION 3.29)

set(TEMPLATE_NAME "Chronica_Regna_Fractorum")
project(${TEMPLATE_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

# -------------------------
# Dependencies
# -------------------------
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)

# -------------------------
# Source files
# -------------------------
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/header)

set(VULKAN_SOURCES
    ${SRC_DIR}/vulkan/Application.cpp
    ${SRC_DIR}/vulkan/Instance.cpp
    ${SRC_DIR}/vulkan/Device.cpp
    ${SRC_DIR}/vulkan/SwapchainPipeline.cpp
    ${SRC_DIR}/vulkan/Buffers.cpp
    ${SRC_DIR}/vulkan/Commands.cpp
    ${SRC_DIR}/vulkan/TextureMapping.cpp
)

add_executable(${TEMPLATE_NAME}
    ${SRC_DIR}/main.cpp
    ${VULKAN_SOURCES}
)

target_compile_features(${TEMPLATE_NAME} PRIVATE cxx_std_20)

# -------------------------
# Include dirs
# -------------------------
target_include_directories(${TEMPLATE_NAME}
    PRIVATE
        ${HEADER_DIR}
        ${HEADER_DIR}/vulkan
        external/stb
        external/tinygltf
        external/tinyobjloader
        external/VMA        # <-- IMPORTANT for vk_mem_alloc.h
)

# -------------------------
# Link libraries
# -------------------------
target_link_libraries(${TEMPLATE_NAME}
    PRIVATE
        Vulkan::Vulkan
        glfw
        glm::glm
)

# -------------------------
# Vulkan defines
# -------------------------
target_compile_definitions(${TEMPLATE_NAME} PRIVATE
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

# -------------------------
# Optional Vulkan C++ module
# -------------------------
if(ENABLE_CPP20_MODULE)
    add_library(VulkanCppModule INTERFACE)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)

    target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
    target_compile_definitions(VulkanCppModule INTERFACE
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
endif()

# -------------------------
# Shader compilation
# -------------------------
find_program(GLSLC glslc)
find_program(SLANGC slangc)

set(SHADER_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})

set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/slang.slang)

if(EXISTS ${SHADER_SOURCE})
    if(SLANGC)
        message(STATUS "Using slangc for shaders")

        add_custom_command(
            OUTPUT ${SHADER_DIR}/slang.spv
            COMMAND ${SLANGC}
                ${SHADER_SOURCE}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -o ${SHADER_DIR}/slang.spv
            DEPENDS ${SHADER_SOURCE}
        )

    elseif(GLSLC)
        message(STATUS "Using glslc for shaders")

        add_custom_command(
            OUTPUT ${SHADER_DIR}/slang.spv
            COMMAND ${GLSLC}
                ${SHADER_SOURCE}
                -o ${SHADER_DIR}/slang.spv
            DEPENDS ${SHADER_SOURCE}
        )

    else()
        message(WARNING "No shader compiler found (slangc or glslc). Skipping shaders.")
    endif()
else()
    message(STATUS "No shader source found — skipping shader build")
endif()

# -------------------------
# Output directory
# -------------------------
set_target_properties(${TEMPLATE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME}
)

# Copy shaders after build
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${CMAKE_BINARY_DIR}/shaders
)
