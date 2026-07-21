#pragma once

#include "Types.hpp"
#include "Platform.hpp"
#include <format>
#include <mutex>
#include <fstream>
#include <source_location>
#include <chrono>
#include <cstdio>

namespace crf {

enum class LogLevel : u8 {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Log {
public:
    static void init(std::string_view filepath);
    static void shutdown();
    static void setMinLevel(LogLevel level);

    template<typename... Args>
    static void trace(std::format_string<Args...> fmt, Args&&... args) {
        if (CRF_UNLIKELY(s_minLevel > LogLevel::Trace)) return;
        log(LogLevel::Trace, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args) {
        if (CRF_UNLIKELY(s_minLevel > LogLevel::Debug)) return;
        log(LogLevel::Debug, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        if (CRF_UNLIKELY(s_minLevel > LogLevel::Info)) return;
        log(LogLevel::Info, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        if (CRF_UNLIKELY(s_minLevel > LogLevel::Warn)) return;
        log(LogLevel::Warn, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        if (CRF_UNLIKELY(s_minLevel > LogLevel::Error)) return;
        log(LogLevel::Error, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Fatal, std::vformat(fmt.get(), std::make_format_args(args...)),
            std::source_location::current());
    }

private:
    static void log(LogLevel level, std::string_view msg,
                    std::source_location loc = std::source_location::current());

    static const char* levelName(LogLevel level);

    inline static std::mutex s_mutex;
    inline static std::ofstream s_file;
    inline static LogLevel s_minLevel = LogLevel::Trace;
    inline static bool s_initialized = false;
};

} // namespace crf
