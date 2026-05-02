#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/World.h"

#include <cassert>
#include <vector>

namespace
{
    struct TestValueComponent
    {
        int value{0};
        int maxValue{0};
    };

    struct TestDeltaComponent
    {
        int value{0};
    };

    void testDestroyRemovesComponents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.addComponent(entity, TestDeltaComponent{5});
        world.addComponent(entity, TestValueComponent{10, 10});

        world.destroyEntity(entity);

        assert(!world.isAlive(entity));
        assert(!world.hasComponent<TestDeltaComponent>(entity));
        assert(!world.hasComponent<TestValueComponent>(entity));
    }

    void testEachVisitsMatchingLiveEntities()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID complete = world.createEntity();
        const Core::EntityID missingDelta = world.createEntity();
        const Core::EntityID destroyed = world.createEntity();

        world.addComponent(complete, TestValueComponent{10, 10});
        world.addComponent(complete, TestDeltaComponent{3});
        world.addComponent(missingDelta, TestValueComponent{20, 20});
        world.addComponent(destroyed, TestValueComponent{30, 30});
        world.addComponent(destroyed, TestDeltaComponent{7});
        world.destroyEntity(destroyed);

        std::vector<Core::EntityID> visited;
        world.each<TestValueComponent, TestDeltaComponent>(
            [&visited](Core::EntityID entity,
                       TestValueComponent& value,
                       const TestDeltaComponent& delta) {
                visited.push_back(entity);
                value.value -= delta.value;
            });

        assert((visited == std::vector<Core::EntityID>{complete}));
        assert(world.getComponent<TestValueComponent>(complete)->value == 7);
        assert(world.getComponent<TestValueComponent>(missingDelta)->value == 20);
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
