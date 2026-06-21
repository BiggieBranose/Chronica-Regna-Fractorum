#include "engine/Engine.hpp"
#include "engine/gameplay/GameLayer.hpp"
#include "engine/rendering/SpriteRenderer.hpp"
#include "engine/rendering/Camera.hpp"
#include "engine/rendering/Texture.hpp"
#include "engine/rendering/Renderer.hpp"
#include "engine/world/Scene.hpp"
#include "engine/world/Entity.hpp"
#include "engine/world/Transform.hpp"
#include "engine/input/Input.hpp"
#include "engine/core/Log.hpp"
#include <cstdlib>

class SandboxLayer : public crf::GameLayer {
public:
    void onAttach(crf::Engine* engine) override {
        GameLayer::onAttach(engine);
        crf::Log::info("SandboxLayer attached");

        // Create some test entities
        auto& scene = engine->getScene();
        for (int i = 0; i < 5; i++) {
            auto entity = scene.createEntity("TestEntity");
            entity->getTransform().setPosition(glm::vec3(
                (float)(i - 2) * 100.0f, 0.0f, 0.0f
            ));
            entity->getTransform().setScale(glm::vec3(64.0f));
        }
    }

    void onUpdate(float dt) override {
        static float elapsed = 0.0f;
        elapsed += dt;

        auto& camera = getEngine()->getCamera();
        float speed = 200.0f;
        auto& input = getEngine()->getInput();

        if (input.isKeyDown(crf::Key::W)) camera.move(glm::vec3(0.0f, speed * dt, 0.0f));
        if (input.isKeyDown(crf::Key::S)) camera.move(glm::vec3(0.0f, -speed * dt, 0.0f));
        if (input.isKeyDown(crf::Key::A)) camera.move(glm::vec3(-speed * dt, 0.0f, 0.0f));
        if (input.isKeyDown(crf::Key::D)) camera.move(glm::vec3(speed * dt, 0.0f, 0.0f));

        // Rotate test entities
        auto& scene = getEngine()->getScene();
        for (auto& e : scene.getEntities()) {
            e->getTransform().rotate(45.0f * dt);
        }
    }

    void onSpriteRender(crf::SpriteRenderer& renderer) override {
        auto& scene = getEngine()->getScene();
        for (auto& entity : scene.getEntities()) {
            auto& t = entity->getTransform();
            glm::vec3 pos = t.getPosition();
            glm::vec3 scale = t.getScale();
            float rot = t.getRotation();
            // Colored quad without texture (texture binding pending)
            auto& tex = m_dummyTex;
            renderer.draw(
                tex,
                glm::vec2(pos.x, pos.y),
                glm::vec2(scale.x, scale.y),
                glm::vec4(1.0f, 0.5f, 0.3f, 1.0f),
                rot
            );
        }

        auto& tex = m_dummyTex;
        renderer.draw(
            tex,
            glm::vec2(0.0f, 0.0f),
            glm::vec2(128.0f, 128.0f),
            glm::vec4(0.3f, 0.5f, 1.0f, 1.0f)
        );
    }

private:
    crf::Texture m_dummyTex;
};

int main() {
    std::atexit([]() { 
        // Ensure cleanup
        glfwTerminate(); 
    });

    auto engine = std::make_unique<crf::Engine>();
    if (!engine->initialize()) {
        crf::Log::error("Engine failed to initialize");
        return EXIT_FAILURE;
    }

    SandboxLayer gameLayer;
    engine->setGameLayer(&gameLayer);
    gameLayer.onAttach(engine.get());

    engine->run();
    engine->shutdown();

    return EXIT_SUCCESS;
}
