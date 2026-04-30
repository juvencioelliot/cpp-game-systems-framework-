#pragma once

#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/Entity.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameCore::Core
{
    class World;

    using PrefabPropertyMap = std::unordered_map<std::string, std::string>;

    struct RuntimePrefabComponent
    {
        using Factory = std::function<void(World&, EntityID, const PrefabPropertyMap&)>;

        std::string type;
        PrefabPropertyMap properties;
        Factory factory;
    };

    struct EntityPrefab
    {
        std::string name;
        std::optional<Components::HealthComponent> health;
        std::optional<Components::AttackComponent> attack;
        std::optional<Components::PositionComponent> position;
        std::vector<RuntimePrefabComponent> runtimeComponents;
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

    class PrefabComponentRegistry
    {
    public:
        using Factory = RuntimePrefabComponent::Factory;

        void registerComponent(std::string type, Factory factory);
        [[nodiscard]] bool hasComponent(const std::string& type) const;
        [[nodiscard]] Factory factoryFor(const std::string& type) const;

    private:
        std::unordered_map<std::string, Factory> m_factories;
    };
}
