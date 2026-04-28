#include "GameCore/Core/EntityPrefab.h"

#include "GameCore/Core/World.h"

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
}
