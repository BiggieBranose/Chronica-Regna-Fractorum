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

bool PhysicsWorld::isCapsuleBlocked(const Capsule& capsule) const {
    const glm::vec2 foot(capsule.base.x, capsule.base.z);
    const f32 top = capsule.base.y + capsule.height;
    for (const AABB& collider : m_colliders) {
        if (collider.min.y > top || collider.max.y < capsule.base.y) {
            continue;
        }
        if (collider.overlapsCircle(foot, capsule.radius)) {
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
