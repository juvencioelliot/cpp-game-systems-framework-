#include "GameCore/Systems/CombatSystem.h"

#include <algorithm>
#include <sstream>

namespace GameCore::Systems
{
    CombatAttackResult CombatSystem::attackDetailed(Core::EntityID attacker,
                                                    Core::EntityID target,
                                                    const AttackStorage& attacks,
                                                    HealthStorage& health)
    {
        CombatAttackResult result;
        result.attacker = attacker;
        result.target = target;

        const auto* attackComponent = attacks.get(attacker);
        auto* targetHealth = health.get(target);

        if (attackComponent == nullptr)
        {
            result.message = "Entity " + std::to_string(attacker) + " cannot attack.";
            return result;
        }
        result.attackerCanAttack = true;

        if (targetHealth == nullptr)
        {
            result.message = "Entity " + std::to_string(target) + " has no health component.";
            return result;
        }
        result.targetHadHealth = true;
        result.targetHealth = targetHealth->currentHealth;
        result.targetMaxHealth = targetHealth->maxHealth;

        if (!targetHealth->isAlive())
        {
            result.message = "Entity " + std::to_string(target) + " is already defeated.";
            return result;
        }

        const int damage = std::max(0, attackComponent->damage);
        targetHealth->currentHealth = std::max(0, targetHealth->currentHealth - damage);
        result.damage = damage;
        result.targetHealth = targetHealth->currentHealth;
        result.damageApplied = true;

        std::ostringstream message;
        message << "Entity " << attacker << " attacks Entity " << target
                << " for " << damage << " damage.";

        if (!targetHealth->isAlive())
        {
            message << " Entity " << target << " is defeated.";
            result.targetDefeated = true;
        }

        result.message = message.str();
        return result;
    }

    std::string CombatSystem::attack(Core::EntityID attacker,
                                     Core::EntityID target,
                                     const AttackStorage& attacks,
                                     HealthStorage& health)
    {
        return attackDetailed(attacker, target, attacks, health).message;
    }
}
