#pragma once
#include <string>
#include <string_view>
#include <source_location>
#include <format>
#include <fstream>
#include <mutex>
#include <utility>

namespace crf {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Log {
public:
    static void init(std::string_view filepath = "engine.log");
    static void shutdown();

    template<typename... Args>
    static void trace(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Trace, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void debug(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Debug, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void info(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Info, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void warn(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Warn, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void error(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Error, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

    template<typename... Args>
    static void fatal(std::string_view fmt, Args&&... args)
    {
        write(LogLevel::Fatal, std::vformat(fmt, std::make_format_args(args...)),
            std::source_location::current());
    }

private:
    static void write(LogLevel level, std::string_view msg, std::source_location loc);

    static std::ofstream s_file;
    static std::mutex    s_mutex;
    static LogLevel      s_minLevel;
};

} // namespace crf
