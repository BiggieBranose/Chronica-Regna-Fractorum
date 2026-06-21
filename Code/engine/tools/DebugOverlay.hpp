#pragma once

namespace crf {

class Engine;

class DebugOverlay {
public:
    DebugOverlay() = default;
    ~DebugOverlay() = default;

    void initialize(Engine* engine);
    void render();

    bool isVisible() const { return m_visible; }
    void setVisible(bool v) { m_visible = v; }

private:
    Engine* m_engine = nullptr;
    bool m_visible = true;
    float m_fpsAccum = 0.0f;
    int m_fpsFrames = 0;
    float m_fpsDisplay = 0.0f;
};

} // namespace crf
