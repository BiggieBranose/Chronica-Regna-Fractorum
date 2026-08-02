#include "game/header/Combat.hpp"

namespace game{
    Combat::Combat(std::vector<Character*> characters, std::vector<Character*> enemies)
    {
        inCombat = true;
        participants.push_back(characters);
        participants.push_back(enemies);

        std::srand(std::time(0));
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

        if(getTeamSize(targetTeam) <= 0){
            crf::Log::info("All targets eliminated. Aborting...");
            return;
        }

        if(attackerTeam == 1) attackerID -= getTeamSize(0);
        else targetID -= getTeamSize(0);

        if (targetID >= getTeamSize(targetTeam)){
            crf::Log::error("Could not find target: out of bounds");
            return;
        }
        else if (targetID < 0 && targetTeam == 1 && targetID + getTeamSize(0) >= 0){
            crf::Log::warn("Target specified but could not be found; defaulting to random target");
            crf::Log::debug("Target expected but could not be found; likely bug in code");
        }


        targetID = getParticipantID(targetTeam, targetID);
        Character* attacker = participants[attackerTeam][attackerID];
        Character* target = participants[targetTeam][targetID];

        // placeholder calc damage
        int damage = calcAttack(attacker, target, 20, DamageType::Meelee);

        target->updateHealth(-damage);
        if(target->getHealth() <= 0){
            participants[targetTeam].erase(participants[targetTeam].begin() + targetID);
        }

    }

    int Combat::getParticipantID(int xPos, int id){
        if (id < 0){
            id = std::rand() % getTeamSize(xPos);
        }
        else if (id >= getTeamSize(xPos)){
            id = getTeamSize(xPos) - 1;
        }
        return id;
    }

    void Combat::loopTurns(){
        while(inCombat){
            int i = 0;
            for(const auto& c : participants[0]){
                runAttack(i, -1);
                i++;
            }

            for (const auto& e : participants[1]){
                runAttack(i, -1);
                i++;
            }

            if(getTeamSize(0) <= 0 || getTeamSize(1) <= 0){
                inCombat = false;
            }
        }
    }
}