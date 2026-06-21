cmake_minimum_required(VERSION 3.29)

set(TEMPLATE_NAME "Chronica_Regna_Fractorum")
project(${TEMPLATE_NAME})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------
# Dependencies
# -------------------------
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED)

# -------------------------
# Source files
# -------------------------
set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/engine)

file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_DIR}/*.cpp"
)

file(GLOB_RECURSE IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp"
)

add_executable(${TEMPLATE_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${ENGINE_SOURCES}
    ${IMGUI_SOURCES}
)

target_compile_features(${TEMPLATE_NAME} PRIVATE cxx_std_20)

# -------------------------
# Include dirs
# -------------------------
target_include_directories(${TEMPLATE_NAME}
    PRIVATE
        ${ENGINE_DIR}
        ${ENGINE_DIR}/core
        ${ENGINE_DIR}/input
        ${ENGINE_DIR}/rendering
        ${ENGINE_DIR}/world
        ${ENGINE_DIR}/gameplay
        ${ENGINE_DIR}/assets
        ${ENGINE_DIR}/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/external/VMA
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tinyobjloader
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tinygltf
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
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
# Shader compilation
# -------------------------
find_program(GLSLC glslc)
find_program(SLANGC slangc)

set(SHADER_DIR ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})

set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang)

if(EXISTS ${SHADER_SOURCE})
    if(SLANGC)
        message(STATUS "Using slangc for shaders")

        add_custom_command(
            OUTPUT ${SHADER_DIR}/graphics.spv
            COMMAND ${SLANGC}
                ${SHADER_SOURCE}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -entry vertMain -stage vertex
                -entry fragMain -stage fragment
                -o ${SHADER_DIR}/graphics.spv
            DEPENDS ${SHADER_SOURCE}
        )

    elseif(GLSLC)
        message(WARNING "glslc cannot compile Slang shaders. Use slangc.")
    else()
        message(WARNING "No shader compiler found (slangc). Skipping shaders.")
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
            ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME}/shaders
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/textures DESTINATION ${CMAKE_BINARY_DIR}/${TEMPLATE_NAME})
