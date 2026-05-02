#pragma once

#include "GameCore/Core/Entity.h"

#include <queue>
#include <vector>

namespace GameCore::Core
{
    class EntityManager
    {
    public:
        EntityID createEntity();
        void destroyEntity(EntityID entity);

        [[nodiscard]] bool isAlive(EntityID entity) const;
        [[nodiscard]] std::size_t livingCount() const;

    private:
        struct EntitySlot
        {
            std::uint32_t generation{1};
            bool alive{false};
        };

        std::vector<EntitySlot> m_slots;
        std::queue<std::uint32_t> m_recycledIndices;
        std::size_t m_livingCount{0};
    };
}
