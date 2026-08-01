#include "game/header/Combat.hpp"
#include "game/header/Character.hpp"

namespace game{
    Combat::Combat(std::vector<Character*> characters, std::vector<Character*> enemies)
    {
        inCombat = true;
        participants.push_back(characters);
        participants.push_back(enemies);
    }

    Combat::~Combat()
    {

    }

    int Combat::calcAttack(Character *character, Character *target, int damage, DamageType type){
        damage = damage * character->getDamageModifier(type);
        damage -= damage * target->getDefenseModifier(type) + target->getDefense();
        if(damage < 0) damage = 0;
        return damage;
    }
    void Combat::runAttack(int attackerID, int targetID){
        int attackerTeam;
        if (attackerID >= getTeamSize(0) && attackerID < getTeamSize(0) + getTeamSize(1)) attackerTeam = 1;
        else if (attackerID < getTeamSize(0) && attackerID >= 0) attackerTeam = 0;
        else {
            crf::Log::error("Could not find target; out of bounds");
            return;
        }

        int targetTeam = (attackerTeam == 1) ? 0 : 1;

        if(attackerTeam == 1) attackerID -= getTeamSize(0);
        else targetID -= getTeamSize(0);

        if (targetID >= getTeamSize(targetTeam)){
            crf::Log::error("Could not find target: out of bounds");
            return;
        }

        Character* attacker = participants[attackerTeam][attackerID];
        Character* target = participants[targetTeam][targetID];

        // placeholder calc damage
        int damage = calcAttack(attacker, target, 20, DamageType::Meelee);
        
        target->updateHealth(-damage);

    }

    Character* Combat::getParticipant(bool isEnemy, int id){
        int xPos = isEnemy ? 1 : 0;

        if (id < 0){
            std::srand(std::time(0));
            id = std::rand() % getTeamSize(xPos);
        }
        else if (id >= getTeamSize(xPos)){
            id = getTeamSize(xPos) - 1;
        }
        return participants[xPos][id];
    }

    void Combat::loopTurns(){
        while(inCombat){
            for(const auto& c : participants[0]){
                // Loop through each character first
            }

            for (const auto& e : participants[1]){
                // loop through enemies second
            }

            if(getTeamSize(0) <= 0 || getTeamSize(1) <= 0){
                inCombat = false;
            }
        }
    }
}