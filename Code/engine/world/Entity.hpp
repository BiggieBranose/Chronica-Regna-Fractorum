#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace crf {

class Transform;

class Entity {
public:
    explicit Entity(std::string name = "Entity");
    ~Entity();

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    uint64_t getId() const { return m_id; }
    const std::string& getName() const { return m_name; }
    void setName(std::string_view name) { m_name = name; }

    Transform& getTransform() { return *m_transform; }
    const Transform& getTransform() const { return *m_transform; }

    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        m_components.push_back(std::move(comp));
        return ptr;
    }

    template<typename T>
    T* getComponent() {
        for (auto& c : m_components) {
            auto* casted = dynamic_cast<T*>(c.get());
            if (casted) return casted;
        }
        return nullptr;
    }

    void update(float dt);
    void render();

private:
    static uint64_t s_nextId;
    uint64_t m_id;
    std::string m_name;
    std::unique_ptr<Transform> m_transform;
    std::vector<std::unique_ptr<class Component>> m_components;
};

} // namespace crf
