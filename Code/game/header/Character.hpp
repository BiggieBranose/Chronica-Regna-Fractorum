#pragma once

namespace game {

class Character {
public:
    void update(float dt, bool w, bool a, bool s, bool d, float terrainHeightAt(float x, float z));

    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getZ() const { return m_z; }
    float getAngle() const { return m_angle; }

private:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_angle = 0.0f;
};

}
