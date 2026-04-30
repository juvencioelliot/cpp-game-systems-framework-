#include "GameCore/Systems/AttackIntentSystem.h"

#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/AttackIntentComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Core/World.h"
#include "GameCore/Systems/CombatEvents.h"

#include <vector>

namespace GameCore::Systems
{
    void AttackIntentSystem::update(Core::World& world, const Core::FrameContext&)
    {
        std::vector<Core::EntityID> defeatedEntities;

        world.each<Components::AttackIntentComponent>(
            [this, &world, &defeatedEntities](Core::EntityID attacker,
                                              Components::AttackIntentComponent& intent) {
                if (intent.target == Core::InvalidEntity || !world.isAlive(attacker))
                {
                    intent.target = Core::InvalidEntity;
                    return;
                }

                auto& attacks = world.storage<Components::AttackComponent>();
                auto& health = world.storage<Components::HealthComponent>();
                const auto result = m_combat.attackDetailed(attacker, intent.target, attacks, health);

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

                intent.target = Core::InvalidEntity;
            });

        for (const auto entity : defeatedEntities)
        {
            world.destroyEntity(entity);
        }
    }
}
