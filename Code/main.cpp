#include <core/Log.hpp>
#include <core/Config.hpp>
#include <graphics/Window.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    crf::Log::init("engine.log");
    crf::Log::info("Engine v0.1.0 starting");

    crf::WindowConfig wc;
    wc.title = "Chronica Regna Fractorum";
    wc.width = 1280;
    wc.height = 720;
    wc.vsync = true;

    crf::Window window(wc);

    auto& cfg = crf::Config::instance();
    cfg.set("window_width", "1280");
    cfg.set("window_height", "720");
    crf::Log::info("Config: {} x {}", cfg.getInt("window_width"), cfg.getInt("window_height"));

    while (!window.shouldClose()) {
        window.pollEvents();

        if (window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (window.wasResized()) {
            crf::Log::info("Resized to {}x{}", window.getWidth(), window.getHeight());
            window.clearResized();
        }
    }

    crf::Log::info("Engine shutdown");
    crf::Log::shutdown();
    return 0;
}
