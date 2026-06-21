#include "Log.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace crf {

std::ofstream Log::s_file;
std::mutex    Log::s_mutex;
LogLevel      Log::s_minLevel = LogLevel::Trace;

static constexpr const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

void Log::init(std::string_view filepath) {
    s_file.open(filepath.data(), std::ios::out | std::ios::trunc);
    if (!s_file.is_open())
        std::cerr << "Failed to open log file: " << filepath << std::endl;
    info("Log system initialized");
}

void Log::shutdown() {
    info("Log system shutting down");
    if (s_file.is_open())
        s_file.close();
}

void Log::write(LogLevel level, std::string_view msg, std::source_location loc) {
    if (level < s_minLevel) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::lock_guard lock(s_mutex);

    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);
    auto line = std::format("[{}.{:03d}] [{}] {} ({}:{})",
        timebuf, ms.count(), levelToString(level),
        msg, loc.file_name(), loc.line());

    std::cout << line << std::endl;
    if (s_file.is_open()) {
        s_file << line << std::endl;
        s_file.flush();
    }
}

} // namespace crf
