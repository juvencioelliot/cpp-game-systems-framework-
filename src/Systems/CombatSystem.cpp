#include "GameCore/Systems/CombatSystem.h"

#include <algorithm>
#include <sstream>

namespace GameCore::Systems
{
    std::string CombatSystem::attack(Core::EntityID attacker,
                                     Core::EntityID target,
                                     const AttackStorage& attacks,
                                     HealthStorage& health)
    {
        const auto* attackComponent = attacks.get(attacker);
        auto* targetHealth = health.get(target);

        if (attackComponent == nullptr)
        {
            return "Entity " + std::to_string(attacker) + " cannot attack.";
        }

        if (targetHealth == nullptr)
        {
            return "Entity " + std::to_string(target) + " has no health component.";
        }

        if (!targetHealth->isAlive())
        {
            return "Entity " + std::to_string(target) + " is already defeated.";
        }

        const int damage = std::max(0, attackComponent->damage);
        targetHealth->currentHealth = std::max(0, targetHealth->currentHealth - damage);

        std::ostringstream message;
        message << "Entity " << attacker << " attacks Entity " << target
                << " for " << damage << " damage.";

        if (!targetHealth->isAlive())
        {
            message << " Entity " << target << " is defeated.";
        }

        return message.str();
    }
}
