#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Entity.hpp"

namespace crf {

class Camera;

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    Entity* createEntity(std::string name = "Entity");
    void destroyEntity(Entity* entity);
    void destroyEntity(uint64_t id);

    Entity* findEntity(uint64_t id);
    Entity* findEntity(std::string_view name);
    const std::vector<std::unique_ptr<Entity>>& getEntities() const { return m_entities; }

    void update(float dt);
    void render();

    void clear();

private:
    std::vector<std::unique_ptr<Entity>> m_entities;
};

} // namespace crf
