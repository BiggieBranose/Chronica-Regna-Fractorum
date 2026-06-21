#pragma once
#include <memory>
#include <GLFW/glfw3.h>

namespace crf {

class Renderer;
class Input;
class Camera;
class Scene;
class GameLayer;
class ImGuiLayer;
class DebugOverlay;
class AssetManager;

class Engine {
public:
    Engine();
    ~Engine();

    bool initialize();
    void run();
    void shutdown();

    Renderer& getRenderer() { return *m_renderer; }
    Input& getInput() { return *m_input; }
    Camera& getCamera() { return *m_camera; }
    Scene& getScene() { return *m_scene; }
    DebugOverlay& getDebugOverlay() { return *m_debugOverlay; }
    AssetManager& getAssetManager() { return *m_assetManager; }

    void setGameLayer(GameLayer* layer) { m_gameLayer = layer; }

    float getDeltaTime() const { return m_deltaTime; }
    bool isRunning() const { return m_running; }

private:
    GLFWwindow* m_window = nullptr;
    bool m_running = false;
    float m_deltaTime = 0.0f;

    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<ImGuiLayer> m_imGuiLayer;
    std::unique_ptr<DebugOverlay> m_debugOverlay;
    std::unique_ptr<AssetManager> m_assetManager;

    GameLayer* m_gameLayer = nullptr;
};

} // namespace crf
