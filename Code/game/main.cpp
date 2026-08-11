#include <game/Game.hpp>

#include <string>

int main(int argc, char* argv[]) {
    bool freeCamera = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--camera" || arg == "-c") {
            freeCamera = true;
        }
    }

    crf::Game game(freeCamera);
    return game.run();
}
