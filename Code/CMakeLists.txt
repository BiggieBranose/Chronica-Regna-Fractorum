cmake_minimum_required(VERSION 3.20)
project(Chronica_Regna_Fractorum VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CRF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

find_package(Vulkan REQUIRED)

add_subdirectory(engine)

add_executable(crf_game main.cpp game/src/Game.cpp)
target_link_libraries(crf_game PRIVATE crf_core crf_graphics)
target_include_directories(crf_game PRIVATE ${CMAKE_SOURCE_DIR})

# Copy assets and shaders to output
add_custom_command(TARGET crf_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:crf_game>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:crf_game>/shaders"
)
