#include "Profiler.hpp"

namespace crf {

Profiler& Profiler::instance() {
    static Profiler prof;
    return prof;
}

void Profiler::beginFrame() {}

void Profiler::endFrame() {}

void Profiler::beginSample(std::string_view name) {
    m_timers.push(std::chrono::high_resolution_clock::now());
    m_currentFrame = name;
}

void Profiler::endSample() {
    if (m_timers.empty()) return;
    auto end = std::chrono::high_resolution_clock::now();
    auto start = m_timers.top(); m_timers.pop();
    double dt = std::chrono::duration<double, std::milli>(end - start).count();
    auto& data = m_samples[m_currentFrame];
    data.time = dt;
    data.count++;
    if (dt < data.min || data.min == 0.0) data.min = dt;
    if (dt > data.max) data.max = dt;
}

} // namespace crf
