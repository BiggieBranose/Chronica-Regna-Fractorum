#pragma once

namespace crf {

class Entity;

class Component {
public:
    virtual ~Component() = default;

    virtual void onAttach(Entity* owner) { m_owner = owner; }
    virtual void onDetach() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onRender() {}

    Entity* getOwner() const { return m_owner; }
    bool isAttached() const { return m_owner != nullptr; }

private:
    Entity* m_owner = nullptr;
};

} // namespace crf
