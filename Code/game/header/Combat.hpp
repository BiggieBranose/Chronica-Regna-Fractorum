#pragma once

#include "Character.hpp"

namespace game{

    class Combat
    {

    public:
        Combat(/* args */);
        ~Combat();
        void calcAttack(Character *character, Character *target, int damage, DamageType type);
    private:
        
    };
}