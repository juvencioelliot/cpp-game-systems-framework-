#pragma once

#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"
#include "GameCore/Core/EntityManager.h"
#include "GameCore/Core/EventBus.h"

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace GameCore::Core
{
    class World
    {
    public:
        EntityID createEntity()
        {
            return m_entities.createEntity();
        }

        void destroyEntity(EntityID entity)
        {
            if (!m_entities.isAlive(entity))
            {
                return;
            }

            for (auto& storage : m_componentStorages)
            {
                storage.second->remove(entity);
            }

            m_entities.destroyEntity(entity);
        }

        [[nodiscard]] bool isAlive(EntityID entity) const
        {
            return m_entities.isAlive(entity);
        }

        [[nodiscard]] std::size_t livingCount() const
        {
            return m_entities.livingCount();
        }

        EventBus& events()
        {
            return m_events;
        }

        const EventBus& events() const
        {
            return m_events;
        }

        template <typename Component>
        Component& addComponent(EntityID entity, const Component& component)
        {
            requireLivingEntity(entity);

            auto& componentStorage = storage<Component>();
            componentStorage.add(entity, component);
            return *componentStorage.get(entity);
        }

        template <typename Component>
        void removeComponent(EntityID entity)
        {
            storage<Component>().remove(entity);
        }

        template <typename Component>
        [[nodiscard]] bool hasComponent(EntityID entity) const
        {
            const auto* componentStorage = findStorage<Component>();
            return componentStorage != nullptr && componentStorage->has(entity);
        }

        template <typename Component>
        Component* getComponent(EntityID entity)
        {
            auto* componentStorage = findStorage<Component>();
            return componentStorage == nullptr ? nullptr : componentStorage->get(entity);
        }

        template <typename Component>
        const Component* getComponent(EntityID entity) const
        {
            const auto* componentStorage = findStorage<Component>();
            return componentStorage == nullptr ? nullptr : componentStorage->get(entity);
        }

        template <typename Component>
        ComponentStorage<Component>& storage()
        {
            const std::type_index key{typeid(Component)};
            auto it = m_componentStorages.find(key);

            if (it == m_componentStorages.end())
            {
                auto inserted = m_componentStorages.emplace(
                    key,
                    std::make_unique<ComponentStorage<Component>>());
                it = inserted.first;
            }

            return static_cast<ComponentStorage<Component>&>(*it->second);
        }

    private:
        template <typename Component>
        [[nodiscard]] ComponentStorage<Component>* findStorage()
        {
            const std::type_index key{typeid(Component)};
            auto it = m_componentStorages.find(key);
            if (it == m_componentStorages.end())
            {
                return nullptr;
            }

            return static_cast<ComponentStorage<Component>*>(it->second.get());
        }

        template <typename Component>
        [[nodiscard]] const ComponentStorage<Component>* findStorage() const
        {
            const std::type_index key{typeid(Component)};
            auto it = m_componentStorages.find(key);
            if (it == m_componentStorages.end())
            {
                return nullptr;
            }

            return static_cast<const ComponentStorage<Component>*>(it->second.get());
        }

        void requireLivingEntity(EntityID entity) const
        {
            if (!m_entities.isAlive(entity))
            {
                throw std::invalid_argument("Cannot add a component to an entity that is not alive.");
            }
        }

        EntityManager m_entities;
        EventBus m_events;
        std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_componentStorages;
    };
}
