#pragma once

#include "GameCore/Core/Entity.h"

#include <queue>
#include <unordered_set>

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
        EntityID m_nextEntity{1};
        std::queue<EntityID> m_recycledEntities;
        std::unordered_set<EntityID> m_livingEntities;
    };
}
