#include <core/Log.hpp>
#include <core/File.hpp>
#include <core/Config.hpp>

int main() {
    crf::Log::init("engine.log");
    crf::Log::info("Engine v0.1.0 starting");

    auto data = crf::File::readBinary("assets/textures/test_tex.png");
    if (data) {
        crf::Log::info("Loaded texture: {} bytes", data->size());
    } else {
        crf::Log::warn("Texture not found");
    }

    auto& cfg = crf::Config::instance();
    cfg.set("window_width", "1280");
    cfg.set("window_height", "720");
    crf::Log::info("Config: {} x {}", cfg.getInt("window_width"), cfg.getInt("window_height"));

    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
