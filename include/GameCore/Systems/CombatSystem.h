#pragma once

#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"

#include <string>

namespace GameCore::Systems
{
    struct CombatAttackResult
    {
        Core::EntityID attacker{Core::InvalidEntity};
        Core::EntityID target{Core::InvalidEntity};
        int damage{0};
        int targetHealth{0};
        int targetMaxHealth{0};
        bool attackerCanAttack{false};
        bool targetHadHealth{false};
        bool damageApplied{false};
        bool targetDefeated{false};
        std::string message;
    };

    class CombatSystem
    {
    public:
        using AttackStorage = Core::ComponentStorage<Components::AttackComponent>;
        using HealthStorage = Core::ComponentStorage<Components::HealthComponent>;

        CombatAttackResult attackDetailed(Core::EntityID attacker,
                                          Core::EntityID target,
                                          const AttackStorage& attacks,
                                          HealthStorage& health);

        std::string attack(Core::EntityID attacker,
                           Core::EntityID target,
                           const AttackStorage& attacks,
                           HealthStorage& health);
    };
}
