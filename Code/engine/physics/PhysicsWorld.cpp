#include "PhysicsWorld.hpp"

namespace crf {

bool PhysicsWorld::isBlocked(glm::vec2 pos, f32 radius) const {
    for (const AABB& collider : m_colliders) {
        if (collider.overlapsCircle(pos, radius)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> PhysicsWorld::overlappingTriggers(const AABB& box) const {
    std::vector<std::string> names;
    for (const Trigger& trigger : m_triggers) {
        if (trigger.bounds.overlaps(box)) {
            names.push_back(trigger.name);
        }
    }
    return names;
}

}
