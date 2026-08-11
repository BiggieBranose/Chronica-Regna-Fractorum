#pragma once

namespace crf {

// Owns the window, renderer, scene and the main loop.
class Game {
public:
    explicit Game(bool freeCamera);

    int run();

private:
    bool m_freeCamera;
};

}
