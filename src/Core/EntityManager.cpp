#include "GameCore/Core/EntityManager.h"

namespace GameCore::Core
{
    EntityID EntityManager::createEntity()
    {
        EntityID entity = InvalidEntity;

        if (!m_recycledEntities.empty())
        {
            entity = m_recycledEntities.front();
            m_recycledEntities.pop();
        }
        else
        {
            entity = m_nextEntity++;
        }

        m_livingEntities.insert(entity);
        return entity;
    }

    void EntityManager::destroyEntity(EntityID entity)
    {
        if (entity == InvalidEntity)
        {
            return;
        }

        const auto removed = m_livingEntities.erase(entity);
        if (removed > 0)
        {
            m_recycledEntities.push(entity);
        }
    }

    bool EntityManager::isAlive(EntityID entity) const
    {
        return m_livingEntities.find(entity) != m_livingEntities.end();
    }

    std::size_t EntityManager::livingCount() const
    {
        return m_livingEntities.size();
    }
}
