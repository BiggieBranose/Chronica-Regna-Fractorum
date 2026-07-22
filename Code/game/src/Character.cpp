#include "../header/Character.hpp"


namespace game {

Character::Character(){
    m_health = m_maxHealth;
    m_defense = 0;

    for (int i = 0; i < static_cast<int>(DamageType::Count); ++i)
        m_damageModifiers[static_cast<DamageType>(i)] = 0.0f;
    
    for (int i = 0; i < static_cast<int>(DamageType::Count); ++i)
        m_defenseModifiers[static_cast<DamageType>(i)] = 0.0f;
}

void Character::setHealth(int health){
    if(health > m_maxHealth) health = m_maxHealth;
    m_health = health;
}
void Character::updateHealth(int difference){
    if((difference + m_health) > m_maxHealth) difference = m_maxHealth - m_health;
    m_health += difference;
}

float Character::getDamageModifier(DamageType type) const {
    auto it = m_damageModifiers.find(type);
    return (it != m_damageModifiers.end()) ? it->second : 0.0f;
}

void Character::setDamageModifier(DamageType type, float value) {
    m_damageModifiers[type] = value;
}

float Character::getDefenseModifier(DamageType type) const {
    auto it = m_defenseModifiers.find(type);
    return (it != m_defenseModifiers.end()) ? it->second : 0.0f;
}

void Character::setDefenseModifier(DamageType type, float value) {
    m_defenseModifiers[type] = value;
}


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
