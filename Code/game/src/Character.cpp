#include "../header/Character.hpp"
#include <cmath>

namespace game {

void Character::update(float dt, bool w, bool a, bool s, bool d, float terrainHeightAt(float, float)) {
    float speed = 3.0f;
    float dx = 0.0f, dz = 0.0f;

    if (w) dz -= speed * dt;
    if (s) dz += speed * dt;
    if (a) dx -= speed * dt;
    if (d) dx += speed * dt;

    if (dx != 0.0f || dz != 0.0f) {
        m_x += dx;
        m_z += dz;
        m_angle = atan2f(dx, -dz);
    }

    m_y = terrainHeightAt(m_x, m_z);
}

}
