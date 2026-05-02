#pragma once

#include "GameCore/Core/Entity.h"

namespace GameCore::Components
{
    struct TransformComponent
    {
        float localX{0.0F};
        float localY{0.0F};
        float rotationRadians{0.0F};
        float scaleX{1.0F};
        float scaleY{1.0F};
        Core::EntityID parent{Core::InvalidEntity};
    };

    struct WorldTransformComponent
    {
        float x{0.0F};
        float y{0.0F};
        float rotationRadians{0.0F};
        float scaleX{1.0F};
        float scaleY{1.0F};
    };
}
