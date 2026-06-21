#include "DebugOverlay.hpp"
#include "../Engine.hpp"
#include "../rendering/Renderer.hpp"
#include "../core/Log.hpp"
#include <cstdio>

namespace crf {

void DebugOverlay::initialize(Engine* engine) {
    m_engine = engine;
    Log::info("DebugOverlay initialized");
}

void DebugOverlay::render() {
    if (!m_visible || !m_engine) return;

    m_fpsAccum += m_engine->getDeltaTime();
    m_fpsFrames++;

    if (m_fpsAccum >= 1.0f) {
        m_fpsDisplay = static_cast<float>(m_fpsFrames) / m_fpsAccum;
        m_fpsAccum = 0.0f;
        m_fpsFrames = 0;
    }

    // Console FPS display for now
    static float fpsLogTimer = 0.0f;
    fpsLogTimer += m_engine->getDeltaTime();
    if (fpsLogTimer >= 5.0f) {
        Log::info("FPS: {:.1f}", m_fpsDisplay);
        fpsLogTimer = 0.0f;
    }
}

} // namespace crf
