cmake_minimum_required(VERSION 3.29)

set(TEMPLATE_NAME "Chronica_Regna_Fractorum")
project(${TEMPLATE_NAME})

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
# Executable
# -------------------------
add_executable(${TEMPLATE_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)

target_compile_features(${TEMPLATE_NAME} PRIVATE cxx_std_20)

target_link_libraries(${TEMPLATE_NAME}
  PRIVATE
    Vulkan::Vulkan
    glfw
)

# -------------------------
# Include dirs
# -------------------------
target_include_directories(${TEMPLATE_NAME}
  PRIVATE
    external/stb
    external/tinygltf
    external/tinyobjloader
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
# Shader compilation (SAFE / OPTIONAL)
# -------------------------

# Try Vulkan SDK glslc first (recommended)
find_program(GLSLC glslc)

# Optional Slang
find_program(SLANGC slangc)

set(SHADER_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})

# Default shader source (only if you actually have it)
set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shader.slang)

if(EXISTS ${SHADER_SOURCE})
  if(SLANGC)
    message(STATUS "Using slangc for shaders")

    add_custom_command(
      OUTPUT ${SHADER_DIR}/shader.spv
      COMMAND ${SLANGC}
        ${SHADER_SOURCE}
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -o ${SHADER_DIR}/shader.spv
      DEPENDS ${SHADER_SOURCE}
    )

  elseif(GLSLC)
    message(STATUS "Using glslc for shaders")

    add_custom_command(
      OUTPUT ${SHADER_DIR}/shader.spv
      COMMAND ${GLSLC}
        ${SHADER_SOURCE}
        -o ${SHADER_DIR}/shader.spv
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