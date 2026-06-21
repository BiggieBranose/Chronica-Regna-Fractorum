add_library(crf_core STATIC
    Log.cpp
    File.cpp
    Config.cpp
)

target_include_directories(crf_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(crf_core PUBLIC cxx_std_20)

if (CRF_WARNINGS_AS_ERRORS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(crf_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
