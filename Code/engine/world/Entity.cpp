#include "Entity.hpp"
#include "Component.hpp"
#include "Transform.hpp"

namespace crf {

uint64_t Entity::s_nextId = 1;

Entity::Entity(std::string name)
    : m_id(s_nextId++), m_name(std::move(name))
    , m_transform(std::make_unique<Transform>())
{
}

Entity::~Entity() = default;

void Entity::update(float dt) {
    for (auto& c : m_components)
        c->onUpdate(dt);
}

void Entity::render() {
    for (auto& c : m_components)
        c->onRender();
}

} // namespace crf
