#pragma once

#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"

#include <iosfwd>
#include <string>

namespace GameCore::Systems
{
    class RenderSystem
    {
    public:
        using HealthStorage = Core::ComponentStorage<Components::HealthComponent>;
        using PositionStorage = Core::ComponentStorage<Components::PositionComponent>;

        void renderEntity(std::ostream& output,
                          Core::EntityID entity,
                          const std::string& name,
                          const HealthStorage& health,
                          const PositionStorage& positions) const;

        void renderSeparator(std::ostream& output) const;
    };
}
