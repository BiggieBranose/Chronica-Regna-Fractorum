#pragma once

#include "core/Types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace crf {

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f}; // euler, degrees
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    glm::mat4 toMat4() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        m = glm::scale(m, scale);
        return m;
    }
};

struct AABB {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    glm::vec3 center() const { return (min + max) * 0.5f; }

    // XZ-plane overlap test with a circle of `radius` around `p` (2.5D movement).
    bool overlapsCircle(glm::vec2 p, f32 radius) const {
        f32 cx = glm::clamp(p.x, min.x, max.x);
        f32 cz = glm::clamp(p.y, min.z, max.z);
        f32 dx = p.x - cx;
        f32 dz = p.y - cz;
        return (dx * dx + dz * dz) <= (radius * radius);
    }

    // Full 3D box-vs-box overlap test.
    bool overlaps(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    // Returns this AABB transformed into a new space by the given matrix.
    AABB transformedBy(const glm::mat4& m) const {
        AABB out{glm::vec3(1e30f), glm::vec3(-1e30f)};
        for (int i = 0; i < 8; ++i) {
            glm::vec4 corner(
                (i & 1) ? max.x : min.x,
                (i & 2) ? max.y : min.y,
                (i & 4) ? max.z : min.z,
                1.0f);
            glm::vec3 p = glm::vec3(m * corner);
            out.min = glm::min(out.min, p);
            out.max = glm::max(out.max, p);
        }
        return out;
    }
};

}
