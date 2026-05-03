#include "CombatGameplay.h"

#include "GameCore/Components/ProgressComponent.h"
#include "GameCore/Core/World.h"

#include <cassert>

namespace
{
    void testAttackIntentSystemDefersDefeatedEntityDestruction()
    {
        GameCore::Core::World world;
        const auto attacker = world.createEntity();
        const auto target = world.createEntity();

        world.addComponent(attacker, CombatDemo::AttackComponent{10});
        world.addComponent(attacker, CombatDemo::AttackIntentComponent{target});
        world.addComponent(target, CombatDemo::HealthComponent{5, 5});
        world.addComponent(target, GameCore::Components::ProgressComponent{5, 5});

        bool defeatPublished = false;
        world.events().subscribe<CombatDemo::EntityDefeatedEvent>(
            [&defeatPublished, target](const CombatDemo::EntityDefeatedEvent& event) {
                defeatPublished = event.entity == target;
            });

        CombatDemo::AttackIntentSystem combat;
        combat.update(world, GameCore::Core::FrameContext{});

        assert(defeatPublished);
        assert(world.isAlive(target));
        assert(world.deferredDestroyCount() == 1);
        assert(world.getComponent<CombatDemo::AttackIntentComponent>(attacker)->target ==
               GameCore::Core::InvalidEntity);
        assert(world.getComponent<GameCore::Components::ProgressComponent>(target)->value == 0);

        world.flushDeferredDestruction();

        assert(!world.isAlive(target));
        assert(world.deferredDestroyCount() == 0);
    }
}

int main()
{
    testAttackIntentSystemDefersDefeatedEntityDestruction();
    return 0;
}
