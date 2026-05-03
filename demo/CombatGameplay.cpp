#include "CombatGameplay.h"

#include "GameCore/Components/ProgressComponent.h"
#include "GameCore/Core/World.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace CombatDemo
{
    namespace
    {
        int readInt(const GameCore::Core::PrefabPropertyMap& properties,
                    const std::string& key,
                    const int fallback)
        {
            const auto it = properties.find(key);
            if (it == properties.end())
            {
                return fallback;
            }

            try
            {
                std::size_t parsedCharacters = 0;
                const int value = std::stoi(it->second, &parsedCharacters);
                if (parsedCharacters != it->second.size())
                {
                    throw std::invalid_argument("trailing characters");
                }
                return value;
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("Invalid integer combat prefab property '" + key + "'.");
            }
        }

        void syncProgress(GameCore::Core::World& world, const GameCore::Core::EntityID entity)
        {
            const auto* health = world.getComponent<HealthComponent>(entity);
            auto* progress = world.getComponent<GameCore::Components::ProgressComponent>(entity);
            if (health != nullptr && progress != nullptr)
            {
                progress->value = health->currentHealth;
                progress->maxValue = health->maxHealth;
            }
        }

        std::string displayEntity(const GameCore::Core::EntityID entity)
        {
            return std::to_string(GameCore::Core::entityIndex(entity));
        }
    }

    CombatAttackResult CombatSystem::attackDetailed(GameCore::Core::EntityID attacker,
                                                    GameCore::Core::EntityID target,
                                                    const AttackStorage& attacks,
                                                    HealthStorage& health)
    {
        CombatAttackResult result;
        result.attacker = attacker;
        result.target = target;

        const auto* attackComponent = attacks.get(attacker);
        auto* targetHealth = health.get(target);

        if (attackComponent == nullptr)
        {
            result.message = "Entity " + displayEntity(attacker) + " cannot attack.";
            return result;
        }
        result.attackerCanAttack = true;

        if (targetHealth == nullptr)
        {
            result.message = "Entity " + displayEntity(target) + " has no health component.";
            return result;
        }
        result.targetHadHealth = true;
        result.targetHealth = targetHealth->currentHealth;
        result.targetMaxHealth = targetHealth->maxHealth;

        if (!targetHealth->isAlive())
        {
            result.message = "Entity " + displayEntity(target) + " is already defeated.";
            return result;
        }

        const int damage = std::max(0, attackComponent->damage);
        targetHealth->currentHealth = std::max(0, targetHealth->currentHealth - damage);
        result.damage = damage;
        result.targetHealth = targetHealth->currentHealth;
        result.damageApplied = true;

        std::ostringstream message;
        message << "Entity " << displayEntity(attacker) << " attacks Entity " << displayEntity(target)
                << " for " << damage << " damage.";

        if (!targetHealth->isAlive())
        {
            message << " Entity " << displayEntity(target) << " is defeated.";
            result.targetDefeated = true;
        }

        result.message = message.str();
        return result;
    }

    std::string CombatSystem::attack(GameCore::Core::EntityID attacker,
                                     GameCore::Core::EntityID target,
                                     const AttackStorage& attacks,
                                     HealthStorage& health)
    {
        return attackDetailed(attacker, target, attacks, health).message;
    }

    void AttackIntentSystem::update(GameCore::Core::World& world,
                                    const GameCore::Core::FrameContext&)
    {
        std::vector<GameCore::Core::EntityID> defeatedEntities;

        world.each<AttackIntentComponent>(
            [this, &world, &defeatedEntities](GameCore::Core::EntityID attacker,
                                              AttackIntentComponent& intent) {
                if (intent.target == GameCore::Core::InvalidEntity || !world.isAlive(attacker))
                {
                    intent.target = GameCore::Core::InvalidEntity;
                    return;
                }

                auto& attacks = world.storage<AttackComponent>();
                auto& health = world.storage<HealthComponent>();
                const auto result = m_combat.attackDetailed(attacker, intent.target, attacks, health);
                syncProgress(world, intent.target);

                if (!result.message.empty())
                {
                    world.events().publish(CombatMessageEvent{result.message});
                }

                if (result.damageApplied)
                {
                    world.events().publish(DamageAppliedEvent{
                        result.attacker,
                        result.target,
                        result.damage,
                        result.targetHealth,
                        result.targetMaxHealth,
                    });
                }

                if (result.targetDefeated)
                {
                    world.events().publish(EntityDefeatedEvent{
                        result.target,
                        result.attacker,
                    });
                    defeatedEntities.push_back(result.target);
                }

                intent.target = GameCore::Core::InvalidEntity;
            });

        for (const auto entity : defeatedEntities)
        {
            world.deferDestroyEntity(entity);
        }
    }

    void registerCombatPrefabComponents(GameCore::Core::PrefabComponentRegistry& registry)
    {
        registry.registerComponent("health", [](GameCore::Core::World& world,
                                                const GameCore::Core::EntityID entity,
                                                const GameCore::Core::PrefabPropertyMap& properties) {
            const int maxHealth = readInt(properties, "max", 100);
            const int currentHealth = readInt(properties, "current", maxHealth);
            world.addComponent(entity, HealthComponent{currentHealth, maxHealth});
            world.addComponent(entity, GameCore::Components::ProgressComponent{currentHealth, maxHealth});
        });

        registry.registerComponent("attack", [](GameCore::Core::World& world,
                                                const GameCore::Core::EntityID entity,
                                                const GameCore::Core::PrefabPropertyMap& properties) {
            world.addComponent(entity, AttackComponent{readInt(properties, "damage", 10)});
        });
    }
}
