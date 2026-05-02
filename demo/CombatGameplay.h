#pragma once

#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Entity.h"
#include "GameCore/Core/EntityPrefab.h"
#include "GameCore/Core/System.h"

#include <string>

namespace CombatDemo
{
    struct HealthComponent
    {
        int currentHealth{100};
        int maxHealth{100};

        [[nodiscard]] bool isAlive() const
        {
            return currentHealth > 0;
        }
    };

    struct AttackComponent
    {
        int damage{10};
    };

    struct AttackIntentComponent
    {
        GameCore::Core::EntityID target{GameCore::Core::InvalidEntity};
    };

    struct CombatMessageEvent
    {
        std::string message;
    };

    struct DamageAppliedEvent
    {
        GameCore::Core::EntityID attacker{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID target{GameCore::Core::InvalidEntity};
        int damage{0};
        int remainingHealth{0};
        int maxHealth{0};
    };

    struct EntityDefeatedEvent
    {
        GameCore::Core::EntityID entity{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID defeatedBy{GameCore::Core::InvalidEntity};
    };

    struct CombatAttackResult
    {
        GameCore::Core::EntityID attacker{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID target{GameCore::Core::InvalidEntity};
        int damage{0};
        int targetHealth{0};
        int targetMaxHealth{0};
        bool attackerCanAttack{false};
        bool targetHadHealth{false};
        bool damageApplied{false};
        bool targetDefeated{false};
        std::string message;
    };

    class CombatSystem
    {
    public:
        using AttackStorage = GameCore::Core::ComponentStorage<AttackComponent>;
        using HealthStorage = GameCore::Core::ComponentStorage<HealthComponent>;

        CombatAttackResult attackDetailed(GameCore::Core::EntityID attacker,
                                          GameCore::Core::EntityID target,
                                          const AttackStorage& attacks,
                                          HealthStorage& health);

        std::string attack(GameCore::Core::EntityID attacker,
                           GameCore::Core::EntityID target,
                           const AttackStorage& attacks,
                           HealthStorage& health);
    };

    class AttackIntentSystem final : public GameCore::Core::ISystem
    {
    public:
        void update(GameCore::Core::World& world,
                    const GameCore::Core::FrameContext& context) override;

    private:
        CombatSystem m_combat;
    };

    void registerCombatPrefabComponents(GameCore::Core::PrefabComponentRegistry& registry);
}
