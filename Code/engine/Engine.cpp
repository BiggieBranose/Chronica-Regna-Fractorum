#include "Engine.hpp"
#include <GLFW/glfw3.h>
#include "rendering/Renderer.hpp"
#include "rendering/Camera.hpp"
#include "rendering/SpriteRenderer.hpp"
#include "input/Input.hpp"
#include "world/Scene.hpp"
#include "gameplay/GameLayer.hpp"
#include "tools/ImGuiLayer.hpp"
#include "tools/DebugOverlay.hpp"
#include "assets/AssetManager.hpp"
#include "core/Log.hpp"
#include "core/Config.hpp"
#include <chrono>

namespace crf {

Engine::Engine() {}

Engine::~Engine() { shutdown(); }

bool Engine::initialize() {
    Log::init("engine.log");
    Log::info("Chronica Regna Fractorum Engine v0.1.0");

    Config::instance().load("config.cfg");

    if (!glfwInit()) {
        Log::error("Failed to initialize GLFW");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    int width = Config::instance().getInt("window_width", 1280);
    int height = Config::instance().getInt("window_height", 720);

    m_window = glfwCreateWindow(width, height, "Chronica Regna Fractorum", nullptr, nullptr);
    if (!m_window) {
        Log::error("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->initialize(m_window)) {
        Log::error("Failed to initialize renderer");
        return false;
    }

    m_input = std::make_unique<Input>();
    m_input->init(m_window);

    m_camera = std::make_unique<Camera>();
    m_camera->setSize((float)width, (float)height);

    m_scene = std::make_unique<Scene>();

    m_assetManager = std::make_unique<AssetManager>();

    m_debugOverlay = std::make_unique<DebugOverlay>();
    m_debugOverlay->initialize(this);

    m_imGuiLayer = std::make_unique<ImGuiLayer>();
    m_imGuiLayer->initialize(this);

    Log::info("Engine initialized successfully");
    return true;
}

void Engine::run() {
    m_running = true;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running && !glfwWindowShouldClose(m_window)) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        m_input->beginFrame();
        glfwPollEvents();

        if (m_imGuiLayer)
            m_imGuiLayer->beginFrame();

        if (m_gameLayer)
            m_gameLayer->onUpdate(m_deltaTime);

        m_scene->update(m_deltaTime);

        if (m_gameLayer)
            m_gameLayer->onRender();

        if (m_renderer->beginFrame()) {
            auto& cmd = m_renderer->getCurrentCommandBuffer();

            auto& spriteRenderer = m_renderer->getSpriteRenderer();
            spriteRenderer.beginFrame(cmd, *m_camera);

            if (m_gameLayer)
                m_gameLayer->onSpriteRender(spriteRenderer);

            m_scene->render();

            spriteRenderer.endFrame();

            if (m_debugOverlay)
                m_debugOverlay->render();

            if (m_imGuiLayer)
                m_imGuiLayer->endFrame(cmd);

            m_renderer->endFrame();
        }

        m_input->endFrame();

        if (m_input->isKeyPressed(Key::Escape))
            m_running = false;
    }

    if (m_renderer)
        m_renderer->getQueue().waitIdle();
}

void Engine::shutdown() {
    m_imGuiLayer.reset();
    m_debugOverlay.reset();
    m_assetManager.reset();
    m_scene.reset();
    m_camera.reset();
    m_input.reset();
    m_renderer.reset();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();

    Config::instance().save("config.cfg");
    Log::shutdown();
}

} // namespace crf
