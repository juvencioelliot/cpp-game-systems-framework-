#pragma once

#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"

#include <string>

namespace GameCore::Systems
{
    class CombatSystem
    {
    public:
        using AttackStorage = Core::ComponentStorage<Components::AttackComponent>;
        using HealthStorage = Core::ComponentStorage<Components::HealthComponent>;

        std::string attack(Core::EntityID attacker,
                           Core::EntityID target,
                           const AttackStorage& attacks,
                           HealthStorage& health);
    };
}
