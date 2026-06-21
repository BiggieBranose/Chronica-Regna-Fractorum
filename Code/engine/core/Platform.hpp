#pragma once

#if defined(_WIN32) || defined(_WIN64)
#  define CRF_WINDOWS 1
#elif defined(__linux__)
#  define CRF_LINUX 1
#elif defined(__APPLE__)
#  define CRF_MACOS 1
#else
#  error Unsupported platform
#endif

#if defined(__clang__)
#  define CRF_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
#  define CRF_GCC 1
#elif defined(_MSC_VER)
#  define CRF_MSVC 1
#else
#  error Unsupported compiler
#endif

#if defined(CRF_CLANG) || defined(CRF_GCC)
#  define CRF_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define CRF_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define CRF_LIKELY(x)   (x)
#  define CRF_UNLIKELY(x) (x)
#endif

#if defined(CRF_CLANG) || defined(CRF_GCC)
#  define CRF_UNUSED __attribute__((unused))
#else
#  define CRF_UNUSED
#endif

#define CRF_STRINGIFY(x) #x
#define CRF_TOSTRING(x)  CRF_STRINGIFY(x)
