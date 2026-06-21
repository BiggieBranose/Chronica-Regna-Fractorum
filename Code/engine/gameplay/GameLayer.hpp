#pragma once
#include <string>

namespace crf {

class Engine;
class SpriteRenderer;
class Renderer;
class Scene;

class GameLayer {
public:
    virtual ~GameLayer() = default;

    virtual void onAttach(Engine* engine) { m_engine = engine; }
    virtual void onDetach() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onRender() {}
    virtual void onSpriteRender(class SpriteRenderer& renderer) { (void)renderer; }
    virtual void onImGuiRender() {}

    Engine* getEngine() const { return m_engine; }

protected:
    Engine* m_engine = nullptr;
};

} // namespace crf
