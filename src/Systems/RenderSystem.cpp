#include "GameCore/Systems/RenderSystem.h"

#include <iostream>

namespace GameCore::Systems
{
    void RenderSystem::renderEntity(std::ostream& output,
                                    Core::EntityID entity,
                                    const std::string& name,
                                    const HealthStorage& health,
                                    const PositionStorage& positions) const
    {
        output << name << " [Entity " << entity << "]";

        if (const auto* position = positions.get(entity))
        {
            output << " Position(" << position->x << ", " << position->y << ")";
        }

        if (const auto* healthComponent = health.get(entity))
        {
            output << " Health " << healthComponent->currentHealth
                   << "/" << healthComponent->maxHealth;
        }

        output << '\n';
    }

    void RenderSystem::renderSeparator(std::ostream& output) const
    {
        output << "----------------------------------------\n";
    }
}
