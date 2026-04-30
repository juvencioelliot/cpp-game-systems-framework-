#include "GameCore/Core/EntityPrefab.h"

#include "GameCore/Core/World.h"

#include <stdexcept>
#include <utility>

namespace GameCore::Core
{
    EntityID PrefabInstantiator::instantiate(World& world, const EntityPrefab& prefab)
    {
        const EntityID entity = world.createEntity();

        if (prefab.health.has_value())
        {
            world.addComponent(entity, *prefab.health);
        }

        if (prefab.attack.has_value())
        {
            world.addComponent(entity, *prefab.attack);
        }

        if (prefab.position.has_value())
        {
            world.addComponent(entity, *prefab.position);
        }

        for (const auto& component : prefab.runtimeComponents)
        {
            component.factory(world, entity, component.properties);
        }

        return entity;
    }

    std::vector<EntityID> PrefabInstantiator::instantiateAll(World& world,
                                                            const PrefabDocument& document)
    {
        std::vector<EntityID> entities;
        entities.reserve(document.entities.size());

        for (const auto& prefab : document.entities)
        {
            entities.push_back(instantiate(world, prefab));
        }

        return entities;
    }

    void PrefabComponentRegistry::registerComponent(std::string type, Factory factory)
    {
        if (type.empty())
        {
            throw std::invalid_argument("Prefab component type cannot be empty.");
        }

        if (!factory)
        {
            throw std::invalid_argument("Prefab component factory cannot be empty.");
        }

        m_factories[std::move(type)] = std::move(factory);
    }

    bool PrefabComponentRegistry::hasComponent(const std::string& type) const
    {
        return m_factories.find(type) != m_factories.end();
    }

    PrefabComponentRegistry::Factory PrefabComponentRegistry::factoryFor(const std::string& type) const
    {
        const auto it = m_factories.find(type);
        if (it == m_factories.end())
        {
            return {};
        }

        return it->second;
    }
}
