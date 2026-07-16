#pragma once

#include "Platform.hpp"
#include <cstdlib>
#include <cstdio>

#if defined(CRF_CLANG) || defined(CRF_GCC)
#  define CRF_DEBUG_BREAK() __builtin_trap()
#elif defined(CRF_MSVC)
#  define CRF_DEBUG_BREAK() __debugbreak()
#else
#  define CRF_DEBUG_BREAK() std::abort()
#endif

#define CRF_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "ASSERT FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            std::fflush(stderr); \
            CRF_DEBUG_BREAK(); \
        } \
    } while (false)

#define CRF_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            std::fprintf(stderr, "ASSERT FAILED: %s — %s (%s:%d)\n", #cond, msg, __FILE__, __LINE__); \
            std::fflush(stderr); \
            CRF_DEBUG_BREAK(); \
        } \
    } while (false)

#define CRF_UNREACHABLE() \
    do { \
        std::fprintf(stderr, "UNREACHABLE (%s:%d)\n", __FILE__, __LINE__); \
        std::fflush(stderr); \
        CRF_DEBUG_BREAK(); \
    } while (false)
