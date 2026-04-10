cmake_minimum_required(VERSION 3.16)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(VulkanTest src/main.cpp)

# -------------------------------
# Platform-specific Vulkan linking
# -------------------------------
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

# -------------------------------
# Shader compilation (Windows + Linux)
# -------------------------------

# Find glslc from Vulkan SDK
find_program(GLSLC glslc HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VULKAN_SDK}/bin"
)

if (NOT GLSLC)
    message(FATAL_ERROR "glslc not found! Make sure Vulkan SDK is installed.")
endif()

# List your shader sources
set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/shader.vert
    ${CMAKE_SOURCE_DIR}/shaders/shader.frag
)

# Output directory for compiled SPIR-V
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Compile each shader
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPIRV ${SHADER_OUTPUT_DIR}/${FILE_NAME}.spv)

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME}"
    )

    list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach()

# Create a target so shaders build automatically
add_custom_target(Shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# Ensure VulkanTest builds shaders before running
add_dependencies(VulkanTest Shaders)
