#pragma once
#include "Log.hpp"
#include <cstdlib>
#include <source_location>

namespace crf {

[[noreturn]] inline void assertFail(std::string_view expr, std::source_location loc = std::source_location::current()) {
    Log::fatal("ASSERTION FAILED: {}", expr);
    std::abort();
}

} // namespace crf

#define CRF_ASSERT(expr) \
    do { \
        if (!(expr)) \
            crf::assertFail(#expr); \
    } while (false)

#define CRF_ASSERT_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            crf::Log::fatal("ASSERTION FAILED: {} - {}", #expr, msg); \
            std::abort(); \
        } \
    } while (false)

#define CRF_UNREACHABLE() \
    crf::assertFail("Entered unreachable code")
