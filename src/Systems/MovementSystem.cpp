#include "GameCore/Systems/MovementSystem.h"

#include "GameCore/Components/MoveIntentComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/World.h"

#include <algorithm>

namespace GameCore::Systems
{
    MovementSystem::MovementSystem(const int minX, const int minY, const int maxX, const int maxY)
        : m_minX(minX),
          m_minY(minY),
          m_maxX(maxX),
          m_maxY(maxY)
    {
    }

    void MovementSystem::update(Core::World& world, const Core::FrameContext&)
    {
        world.each<Components::PositionComponent, Components::MoveIntentComponent>(
            [this](Core::EntityID,
                   Components::PositionComponent& position,
                   Components::MoveIntentComponent& moveIntent) {
                position.x = std::clamp(position.x + moveIntent.dx, m_minX, m_maxX);
                position.y = std::clamp(position.y + moveIntent.dy, m_minY, m_maxY);
                moveIntent.dx = 0;
                moveIntent.dy = 0;
            });
    }
}
