add_library(crf_physics STATIC
    PhysicsWorld.cpp
)

target_link_libraries(crf_physics PUBLIC crf_core glm::glm)
target_include_directories(crf_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if (CRF_WARNINGS_AS_ERRORS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crf_physics PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(crf_physics PRIVATE /W4 /WX)
    endif()
endif()
