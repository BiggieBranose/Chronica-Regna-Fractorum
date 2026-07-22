#pragma once

#include <cmath>
#include <unordered_map>


namespace game {

enum class DamageType {
    Meelee,
    Ranged,
    Magical,
    Count
};

class Character {
public:
    Character();
    void update(float dt, bool w, bool a, bool s, bool d, float terrainHeightAt(float x, float z));

    float getX() const { return m_x; }
    float getY() const { return m_y; }
    float getZ() const { return m_z; }
    float getAngle() const { return m_angle; }

    int getHealth() const { return m_health; }
    int getDefense() const { return m_defense; }
    void setHealth(int health);
    void updateHealth(int difference);

    float getDamageModifier(DamageType type) const;
    void setDamageModifier(DamageType type, float value);

    float getDefenseModifier(DamageType type) const;
    void setDefenseModifier(DamageType type, float value);

private:
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_z = 0.0f;
    float m_angle = 0.0f;

    /* Character combat stats */
    int m_maxHealth = 100;
    int m_health;
    int m_defense;
    std::unordered_map<DamageType, float> m_damageModifiers;
    std::unordered_map<DamageType, float> m_defenseModifiers;
};

}
