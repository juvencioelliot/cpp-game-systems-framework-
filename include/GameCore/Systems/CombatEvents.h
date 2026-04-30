#pragma once

#include "GameCore/Core/Entity.h"

#include <string>

namespace GameCore::Systems
{
    struct CombatMessageEvent
    {
        std::string message;
    };

    struct DamageAppliedEvent
    {
        Core::EntityID attacker{Core::InvalidEntity};
        Core::EntityID target{Core::InvalidEntity};
        int damage{0};
        int remainingHealth{0};
        int maxHealth{0};
    };

    struct EntityDefeatedEvent
    {
        Core::EntityID entity{Core::InvalidEntity};
        Core::EntityID defeatedBy{Core::InvalidEntity};
    };
}
