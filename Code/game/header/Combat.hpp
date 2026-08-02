#pragma once

#include "Character.hpp"
#include "engine/core/Log.hpp"

#include <cstdlib>
#include <ctime>
#include <vector>

namespace game{

    class Combat
    {

    public:
        Combat(std::vector<Character*> characters, std::vector<Character*> enemies);
        ~Combat();
    private:
        int calcAttack(Character *character, Character *target, int damage, DamageType type); // Maybe we need a parent-class for Character eventually?? like a Entity class?
        void runAttack(int attackerID, int targetID);
        void loopTurns();

        int getParticipantID(int xPos, int id);

        int getTeamSize(int xPos) { return participants[xPos].size(); }

        bool inCombat = false;
        std::vector<std::vector<Character*>> participants; // Row 1: characters -- Row 2: enemies
    };
}