#pragma once
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <stack>

namespace crf {

class Profiler {
public:
    static Profiler& instance();

    void beginFrame();
    void endFrame();

    void beginSample(std::string_view name);
    void endSample();

    struct SampleData {
        double time = 0.0;
        double min = 0.0;
        double max = 0.0;
        int count = 0;
    };

    const std::unordered_map<std::string, SampleData>& getSamples() const { return m_samples; }

private:
    Profiler() = default;
    std::unordered_map<std::string, SampleData> m_samples;
    std::stack<std::chrono::high_resolution_clock::time_point> m_timers;
    std::string m_currentFrame;
};

class ProfileScope {
public:
    ProfileScope(std::string_view name) : m_name(name) { Profiler::instance().beginSample(m_name); }
    ~ProfileScope() { Profiler::instance().endSample(); }
private:
    std::string m_name;
};

} // namespace crf

#define CRF_PROFILE_SCOPE(name) crf::ProfileScope CRF_CONCAT(prof_, __LINE__)(name)
#define CRF_CONCAT(a, b) a ## b
