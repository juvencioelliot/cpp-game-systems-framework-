#pragma once

#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/Entity.h"

#include <optional>
#include <string>
#include <vector>

namespace GameCore::Core
{
    class World;

    struct EntityPrefab
    {
        std::string name;
        std::optional<Components::HealthComponent> health;
        std::optional<Components::AttackComponent> attack;
        std::optional<Components::PositionComponent> position;
    };

    struct PrefabDocument
    {
        std::vector<EntityPrefab> entities;
    };

    class PrefabInstantiator
    {
    public:
        static EntityID instantiate(World& world, const EntityPrefab& prefab);
        static std::vector<EntityID> instantiateAll(World& world, const PrefabDocument& document);
    };
}
