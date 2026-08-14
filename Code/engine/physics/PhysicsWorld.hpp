#pragma once

#include "core/Types.hpp"
#include "core/Transform.hpp"
#include <string>
#include <vector>

namespace crf {

struct Trigger {
    std::string name;
    AABB bounds;
};

// Owns the static collision world: solid colliders and trigger volumes.
class PhysicsWorld {
public:
    void addCollider(const AABB& bounds) { m_colliders.push_back(bounds); }

    void addTrigger(const std::string& name, const AABB& bounds) {
        m_triggers.push_back({name, bounds});
    }

    // XZ-plane collision query for movement (2.5D).
    bool isBlocked(glm::vec2 pos, f32 radius) const;

    // True if a capsule (circle footprint + vertical span) overlaps any collider.
    bool isCapsuleBlocked(const Capsule& capsule) const;

    // Names of all triggers whose volume overlaps the given box (e.g. an entity's AABB).
    std::vector<std::string> overlappingTriggers(const AABB& box) const;

    const std::vector<AABB>& getColliders() const { return m_colliders; }
    const std::vector<Trigger>& getTriggers() const { return m_triggers; }

private:
    std::vector<AABB> m_colliders;
    std::vector<Trigger> m_triggers;
};

}
