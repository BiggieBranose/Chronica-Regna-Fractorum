cmake_minimum_required(VERSION 3.20)
project(Chronica_Regna_Fractorum VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CRF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

find_package(Vulkan REQUIRED)

add_subdirectory(engine)

# --- Shader compilation ---
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

set(SHADER_SOURCES
    ${SHADER_SRC_DIR}/box.vert
    ${SHADER_SRC_DIR}/box.frag
    ${SHADER_SRC_DIR}/billboard.vert
    ${SHADER_SRC_DIR}/billboard.frag
    ${SHADER_SRC_DIR}/raygen.rgen
    ${SHADER_SRC_DIR}/miss.rmiss
    ${SHADER_SRC_DIR}/closesthit.rchit
)

set(SPIRV_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    get_filename_component(SHADER_BASENAME ${SHADER} NAME_WE)
    get_filename_component(EXT ${SHADER} EXT)

    # RT shaders use basename (raygen.rgen -> raygen.spv), others use full name (box.vert -> box.vert.spv)
    if(EXT STREQUAL ".rgen" OR EXT STREQUAL ".rmiss" OR EXT STREQUAL ".rchit")
        set(SPV_FILE "${SHADER_OUT_DIR}/${SHADER_BASENAME}.spv")
        set(SPV_NAME "${SHADER_BASENAME}.spv")
    else()
        set(SPV_FILE "${SHADER_OUT_DIR}/${SHADER_NAME}.spv")
        set(SPV_NAME "${SHADER_NAME}.spv")
    endif()

    # Determine stage from extension
    if(EXT STREQUAL ".vert")
        set(STAGE "vert")
        set(EXTRA_FLAGS "")
    elseif(EXT STREQUAL ".frag")
        set(STAGE "frag")
        set(EXTRA_FLAGS "")
    elseif(EXT STREQUAL ".rgen")
        set(STAGE "rgen")
        set(EXTRA_FLAGS "--target-env=vulkan1.2")
    elseif(EXT STREQUAL ".rmiss")
        set(STAGE "rmiss")
        set(EXTRA_FLAGS "--target-env=vulkan1.2")
    elseif(EXT STREQUAL ".rchit")
        set(STAGE "rchit")
        set(EXTRA_FLAGS "--target-env=vulkan1.2")
    else()
        set(STAGE "frag")
        set(EXTRA_FLAGS "")
    endif()

    # box.frag uses ray queries, needs vulkan1.2 env
    if(SHADER_NAME STREQUAL "box.frag")
        set(EXTRA_FLAGS "--target-env=vulkan1.2")
    endif()

    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND Vulkan::glslc -fshader-stage=${STAGE} ${EXTRA_FLAGS} -o ${SPV_FILE} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME} -> ${SPV_NAME}"
    )
    list(APPEND SPIRV_OUTPUTS ${SPV_FILE})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# --- Game executable ---
add_executable(crf_game main.cpp)
add_dependencies(crf_game compile_shaders)
target_link_libraries(crf_game PRIVATE crf_core crf_graphics)
target_include_directories(crf_game PRIVATE ${CMAKE_SOURCE_DIR})

# Copy assets and compiled shaders to output
add_custom_command(TARGET crf_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:crf_game>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${SHADER_OUT_DIR}"
        "$<TARGET_FILE_DIR:crf_game>/shaders"
)
