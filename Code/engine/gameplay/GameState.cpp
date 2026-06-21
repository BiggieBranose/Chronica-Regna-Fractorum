#include "GameState.hpp"

namespace crf {

GameStateManager& GameStateManager::instance() {
    static GameStateManager mgr;
    return mgr;
}

} // namespace crf
