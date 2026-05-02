#include "GameCore/Core/EntityManager.h"

#include <stdexcept>

namespace GameCore::Core
{
    EntityID EntityManager::createEntity()
    {
        std::uint32_t index = 0;

        if (!m_recycledIndices.empty())
        {
            index = m_recycledIndices.front();
            m_recycledIndices.pop();
        }
        else
        {
            if (m_slots.size() >= EntityMaxIndex)
            {
                throw std::runtime_error("EntityManager exhausted encodable entity indices.");
            }

            index = static_cast<std::uint32_t>(m_slots.size() + 1);
            m_slots.push_back(EntitySlot{});
        }

        auto& slot = m_slots[static_cast<std::size_t>(index - 1)];
        slot.alive = true;
        ++m_livingCount;
        return makeEntityID(index, slot.generation);
    }

    void EntityManager::destroyEntity(EntityID entity)
    {
        if (!isAlive(entity))
        {
            return;
        }

        const auto index = entityIndex(entity);
        auto& slot = m_slots[static_cast<std::size_t>(index - 1)];
        slot.alive = false;
        slot.generation = (slot.generation % EntityMaxGeneration) + 1;
        if (slot.generation == entityGeneration(entity))
        {
            slot.generation = (slot.generation % EntityMaxGeneration) + 1;
        }
        --m_livingCount;
        m_recycledIndices.push(index);
    }

    bool EntityManager::isAlive(EntityID entity) const
    {
        if (entity == InvalidEntity)
        {
            return false;
        }

        const auto index = entityIndex(entity);
        if (index == 0 || index > m_slots.size())
        {
            return false;
        }

        const auto& slot = m_slots[static_cast<std::size_t>(index - 1)];
        return slot.alive && slot.generation == entityGeneration(entity);
    }

    std::size_t EntityManager::livingCount() const
    {
        return m_livingCount;
    }
}
