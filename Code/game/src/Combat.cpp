#include "../header/Combat.hpp"

namespace game{
    Combat::Combat(/* args */)
    {
        
    }

    Combat::~Combat()
    {

    }

    void Combat::calcAttack(Character *character, Character *target, int damage, DamageType type){
        damage = damage * character->getDamageModifier(type);
        damage -= damage * target->getDefenseModifier(type) + target->getDefense();
        if(damage < 0) damage = 0;
        target->updateHealth(-damage);
    }
}