cmake_minimum_required(VERSION 3.20)
project(CRF VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(Vulkan REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)

# stb and tinyobjloader are header-only — just need their include dirs.
# On Arch:   sudo pacman -S stb tinyobjloader
# On Windows: put the headers somewhere and set STB_INCLUDE_DIR / TINYOBJ_INCLUDE_DIR
#             via -DSTB_INCLUDE_DIR="C:/path/to/stb" etc.

if(NOT DEFINED STB_INCLUDE_DIR)
    # Arch Linux default pacman install path
    set(STB_INCLUDE_DIR "/usr/include")
endif()

if(NOT DEFINED TINYOBJ_INCLUDE_DIR)
    set(TINYOBJ_INCLUDE_DIR "/usr/include")
endif()

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------

add_executable(CRF main.cpp)

target_include_directories(CRF PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${STB_INCLUDE_DIR}
    ${TINYOBJ_INCLUDE_DIR}
)

target_link_libraries(CRF PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# On Windows with MSVC disable the min/max macros that conflict with std::clamp
if(MSVC)
    target_compile_definitions(CRF PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# Shader compilation
# ---------------------------------------------------------------------------
# Compiles shaders/shader.vert and shaders/shader.frag → shaders/vert.spv / frag.spv
# glslc is shipped with the Vulkan SDK on both platforms.

find_program(GLSLC glslc HINTS
    "$ENV{VULKAN_SDK}/Bin"       # Windows SDK default
    "$ENV{VULKAN_SDK}/bin"       # Linux SDK default
    "/usr/bin"                   # Arch pacman vulkan-tools
)

if(GLSLC)
    set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
    set(VERT_SRC   ${SHADER_DIR}/shader.vert)
    set(FRAG_SRC   ${SHADER_DIR}/shader.frag)
    set(VERT_SPV   ${SHADER_DIR}/vert.spv)
    set(FRAG_SPV   ${SHADER_DIR}/frag.spv)

    add_custom_command(
        OUTPUT  ${VERT_SPV}
        COMMAND ${GLSLC} ${VERT_SRC} -o ${VERT_SPV}
        DEPENDS ${VERT_SRC}
        COMMENT "Compiling vertex shader"
    )
    add_custom_command(
        OUTPUT  ${FRAG_SPV}
        COMMAND ${GLSLC} ${FRAG_SRC} -o ${FRAG_SPV}
        DEPENDS ${FRAG_SRC}
        COMMENT "Compiling fragment shader"
    )
    add_custom_target(Shaders ALL DEPENDS ${VERT_SPV} ${FRAG_SPV})
    add_dependencies(CRF Shaders)
else()
    message(WARNING "glslc not found — compile shaders manually:\n"
                    "  glslc shaders/shader.vert -o shaders/vert.spv\n"
                    "  glslc shaders/shader.frag -o shaders/frag.spv")
endif()
