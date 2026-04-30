#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/World.h"

#include <cassert>
#include <vector>

namespace
{
    void testDestroyRemovesComponents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.addComponent(entity, Components::AttackComponent{5});
        world.addComponent(entity, Components::HealthComponent{10, 10});

        world.destroyEntity(entity);

        assert(!world.isAlive(entity));
        assert(!world.hasComponent<Components::AttackComponent>(entity));
        assert(!world.hasComponent<Components::HealthComponent>(entity));
    }

    void testEachVisitsMatchingLiveEntities()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID complete = world.createEntity();
        const Core::EntityID missingAttack = world.createEntity();
        const Core::EntityID destroyed = world.createEntity();

        world.addComponent(complete, Components::HealthComponent{10, 10});
        world.addComponent(complete, Components::AttackComponent{3});
        world.addComponent(missingAttack, Components::HealthComponent{20, 20});
        world.addComponent(destroyed, Components::HealthComponent{30, 30});
        world.addComponent(destroyed, Components::AttackComponent{7});
        world.destroyEntity(destroyed);

        std::vector<Core::EntityID> visited;
        world.each<Components::HealthComponent, Components::AttackComponent>(
            [&visited](Core::EntityID entity,
                       Components::HealthComponent& health,
                       const Components::AttackComponent& attack) {
                visited.push_back(entity);
                health.currentHealth -= attack.damage;
            });

        assert((visited == std::vector<Core::EntityID>{complete}));
        assert(world.getComponent<Components::HealthComponent>(complete)->currentHealth == 7);
        assert(world.getComponent<Components::HealthComponent>(missingAttack)->currentHealth == 20);
    }

    void testEachSupportsConstIteration()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, Components::PositionComponent{4, 5});

        const Core::World& constWorld = world;
        int total = 0;
        constWorld.each<Components::PositionComponent>(
            [&total](Core::EntityID, const Components::PositionComponent& position) {
                total = position.x + position.y;
            });

        assert(total == 9);
    }
}

int main()
{
    testDestroyRemovesComponents();
    testEachVisitsMatchingLiveEntities();
    testEachSupportsConstIteration();
    return 0;
}
