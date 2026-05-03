#pragma once

#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"
#include "GameCore/Core/EntityManager.h"
#include "GameCore/Core/EventBus.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

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

        void deferDestroyEntity(EntityID entity)
        {
            if (m_entities.isAlive(entity) &&
                std::find(m_deferredDestroyEntities.begin(),
                          m_deferredDestroyEntities.end(),
                          entity) == m_deferredDestroyEntities.end())
            {
                m_deferredDestroyEntities.push_back(entity);
            }
        }

        void flushDeferredDestruction()
        {
            const auto entities = std::move(m_deferredDestroyEntities);
            m_deferredDestroyEntities.clear();

            for (const auto entity : entities)
            {
                destroyEntity(entity);
            }
        }

        template <typename Component>
        void deferAddComponent(EntityID entity, const Component& component)
        {
            if (!m_entities.isAlive(entity))
            {
                return;
            }

            m_deferredComponentMutations.push_back(
                [entity, component](World& world) {
                    if (world.isAlive(entity))
                    {
                        world.addComponent(entity, component);
                    }
                });
        }

        template <typename Component>
        void deferRemoveComponent(EntityID entity)
        {
            m_deferredComponentMutations.push_back(
                [entity](World& world) {
                    world.removeComponent<Component>(entity);
                });
        }

        void flushDeferredComponentMutations()
        {
            auto mutations = std::move(m_deferredComponentMutations);
            m_deferredComponentMutations.clear();

            for (auto& mutation : mutations)
            {
                mutation(*this);
            }
        }

        [[nodiscard]] std::size_t deferredDestroyCount() const
        {
            return m_deferredDestroyEntities.size();
        }

        [[nodiscard]] std::size_t deferredComponentMutationCount() const
        {
            return m_deferredComponentMutations.size();
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
            auto* componentStorage = findStorage<Component>();
            if (componentStorage != nullptr)
            {
                componentStorage->remove(entity);
            }
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

        template <typename FirstComponent, typename... Components, typename Callback>
        void each(Callback callback)
        {
            auto* firstStorage = findStorage<FirstComponent>();
            if (firstStorage == nullptr)
            {
                return;
            }

            for (auto& entry : firstStorage->all())
            {
                const EntityID entity = entry.first;
                if (!isAlive(entity))
                {
                    continue;
                }

                if constexpr (sizeof...(Components) == 0)
                {
                    callback(entity, entry.second);
                }
                else
                {
                    callIfHasComponents<Components...>(entity, entry.second, callback);
                }
            }
        }

        template <typename FirstComponent, typename... Components, typename Callback>
        void each(Callback callback) const
        {
            const auto* firstStorage = findStorage<FirstComponent>();
            if (firstStorage == nullptr)
            {
                return;
            }

            for (const auto& entry : firstStorage->all())
            {
                const EntityID entity = entry.first;
                if (!isAlive(entity))
                {
                    continue;
                }

                if constexpr (sizeof...(Components) == 0)
                {
                    callback(entity, entry.second);
                }
                else
                {
                    callIfHasComponents<Components...>(entity, entry.second, callback);
                }
            }
        }

    private:
        template <typename... Components, typename FirstComponent, typename Callback>
        void callIfHasComponents(EntityID entity, FirstComponent& firstComponent, Callback& callback)
        {
            if (((getComponent<Components>(entity) != nullptr) && ...))
            {
                callback(entity, firstComponent, *getComponent<Components>(entity)...);
            }
        }

        template <typename... Components, typename FirstComponent, typename Callback>
        void callIfHasComponents(EntityID entity,
                                 const FirstComponent& firstComponent,
                                 Callback& callback) const
        {
            if (((getComponent<Components>(entity) != nullptr) && ...))
            {
                callback(entity, firstComponent, *getComponent<Components>(entity)...);
            }
        }

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
        std::vector<std::function<void(World&)>> m_deferredComponentMutations;
        std::vector<EntityID> m_deferredDestroyEntities;
    };
}
