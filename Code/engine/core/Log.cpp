#include "Log.hpp"
#include "Platform.hpp"
#include <iostream>
#include <ctime>

namespace crf {

static std::string timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    auto tm = *std::localtime(&timer);
    return std::format("{:02d}:{:02d}:{:02d}.{:03d}",
                       tm.tm_hour, tm.tm_min, tm.tm_sec,
                       static_cast<int>(ms.count()));
}

void Log::init(std::string_view filepath) {
    std::lock_guard lock(s_mutex);
    if (s_initialized) return;
    s_file.open(filepath.data(), std::ios::out | std::ios::trunc);
    s_initialized = true;
}

void Log::shutdown() {
    std::lock_guard lock(s_mutex);
    if (!s_initialized) return;
    s_file.close();
    s_initialized = false;
}

void Log::setMinLevel(LogLevel level) {
    std::lock_guard lock(s_mutex);
    s_minLevel = level;
}

const char* Log::levelName(LogLevel level) {
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

void Log::log(LogLevel level, std::string_view msg, std::source_location loc) {
    std::lock_guard lock(s_mutex);
    if (!s_initialized) {
        std::fprintf(stderr, "[%s] %s\n", levelName(level), msg.data());
        return;
    }

    auto ts = timestamp();
    auto line = std::format("[{}] [{}] {} ({}:{})",
                            ts, levelName(level), msg,
                            loc.file_name(), loc.line());

    std::cout << line << std::endl;
    s_file << line << std::endl;
    s_file.flush();
}

} // namespace crf
