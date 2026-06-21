#pragma once

namespace crf {

enum class GameState {
    Boot,
    MainMenu,
    Loading,
    Playing,
    Paused,
    GameOver
};

class GameStateManager {
public:
    static GameStateManager& instance();

    GameState getState() const { return m_state; }
    void setState(GameState state) { m_state = state; }
    bool isState(GameState state) const { return m_state == state; }

private:
    GameStateManager() = default;
    GameState m_state = GameState::Boot;
};

} // namespace crf
