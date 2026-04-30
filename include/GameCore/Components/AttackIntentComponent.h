#pragma once

#include "GameCore/Core/Entity.h"

namespace GameCore::Components
{
    struct AttackIntentComponent
    {
        Core::EntityID target{Core::InvalidEntity};
    };
}
