#include "Scene.hpp"
#include "Transform.hpp"
#include <algorithm>

namespace crf {

Entity* Scene::createEntity(std::string name) {
    auto entity = std::make_unique<Entity>(std::move(name));
    Entity* ptr = entity.get();
    m_entities.push_back(std::move(entity));
    return ptr;
}

void Scene::destroyEntity(Entity* entity) {
    if (!entity) return;
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
        [entity](const auto& e) { return e.get() == entity; });
    if (it != m_entities.end())
        m_entities.erase(it);
}

void Scene::destroyEntity(uint64_t id) {
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
        [id](const auto& e) { return e->getId() == id; });
    if (it != m_entities.end())
        m_entities.erase(it);
}

Entity* Scene::findEntity(uint64_t id) {
    for (auto& e : m_entities)
        if (e->getId() == id) return e.get();
    return nullptr;
}

Entity* Scene::findEntity(std::string_view name) {
    for (auto& e : m_entities)
        if (e->getName() == name) return e.get();
    return nullptr;
}

void Scene::update(float dt) {
    for (auto& e : m_entities)
        e->update(dt);
}

void Scene::render() {
    for (auto& e : m_entities)
        e->render();
}

void Scene::clear() {
    m_entities.clear();
}

} // namespace crf
