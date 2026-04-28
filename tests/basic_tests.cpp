#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/Application.h"
#include "GameCore/Core/AssetLoaders.h"
#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/EventBus.h"
#include "GameCore/Core/EntityManager.h"
#include "GameCore/Core/EntityPrefab.h"
#include "GameCore/Core/FileSystem.h"
#include "GameCore/Core/InputManager.h"
#include "GameCore/Core/KeyValuePrefabLoader.h"
#include "GameCore/Core/ResourceManager.h"
#include "GameCore/Core/Scene.h"
#include "GameCore/Core/SceneManager.h"
#include "GameCore/Core/StateMachine.h"
#include "GameCore/Core/SystemScheduler.h"
#include "GameCore/Core/World.h"
#include "GameCore/Systems/CombatSystem.h"
#include "GameCore/Systems/RenderSystem.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct DamageEvent
    {
        GameCore::Core::EntityID entity{GameCore::Core::InvalidEntity};
        int amount{0};
    };

    struct TextResource
    {
        std::string text;
    };

    struct NumberResource
    {
        int value{0};
    };

    std::filesystem::path testAssetPath(const std::string& filename)
    {
        static int counter = 0;
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::string uniqueName = "gamecore_" + std::to_string(timestamp) + "_" +
                                       std::to_string(counter++) + "_" + filename;
        return std::filesystem::temp_directory_path() / uniqueName;
    }

    void writeTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream file(path);
        file << text;
    }

    void writeBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        for (const auto byte : bytes)
        {
            file.put(static_cast<char>(byte));
        }
    }

    enum class TestState
    {
        Idle,
        Running,
        Paused,
    };

    class RecordingSystem final : public GameCore::Core::ISystem
    {
    public:
        RecordingSystem(int id, std::vector<int>& updateOrder)
            : m_id(id),
              m_updateOrder(updateOrder)
        {
        }

        void update(GameCore::Core::World&, const GameCore::Core::FrameContext& context) override
        {
            m_updateOrder.push_back(m_id);
            m_lastDeltaSeconds = context.deltaSeconds;
            m_lastFrameIndex = context.frameIndex;
        }

        [[nodiscard]] float lastDeltaSeconds() const
        {
            return m_lastDeltaSeconds;
        }

        [[nodiscard]] std::uint64_t lastFrameIndex() const
        {
            return m_lastFrameIndex;
        }

    private:
        int m_id{0};
        std::vector<int>& m_updateOrder;
        float m_lastDeltaSeconds{0.0F};
        std::uint64_t m_lastFrameIndex{0};
    };

    class HealthDecaySystem final : public GameCore::Core::ISystem
    {
    public:
        void update(GameCore::Core::World& world, const GameCore::Core::FrameContext&) override
        {
            auto& health = world.storage<GameCore::Components::HealthComponent>();
            for (auto& entry : health.all())
            {
                --entry.second.currentHealth;
            }
        }
    };

    class DamagePublishingSystem final : public GameCore::Core::ISystem
    {
    public:
        explicit DamagePublishingSystem(GameCore::Core::EntityID entity)
            : m_entity(entity)
        {
        }

        void update(GameCore::Core::World& world, const GameCore::Core::FrameContext&) override
        {
            world.events().publish(DamageEvent{m_entity, 3});
        }

    private:
        GameCore::Core::EntityID m_entity{GameCore::Core::InvalidEntity};
    };

    class LifecycleScene final : public GameCore::Core::Scene
    {
    public:
        [[nodiscard]] int initializeCount() const
        {
            return m_initializeCount;
        }

        [[nodiscard]] int updateCount() const
        {
            return m_updateCount;
        }

        [[nodiscard]] int shutdownCount() const
        {
            return m_shutdownCount;
        }

    protected:
        void onInitialize() override
        {
            ++m_initializeCount;
        }

        void onUpdate(const GameCore::Core::FrameContext&) override
        {
            ++m_updateCount;
        }

        void onShutdown() override
        {
            ++m_shutdownCount;
        }

    private:
        int m_initializeCount{0};
        int m_updateCount{0};
        int m_shutdownCount{0};
    };

    class StoppingScene final : public GameCore::Core::Scene
    {
    public:
        explicit StoppingScene(GameCore::Core::Application& application)
            : m_application(application)
        {
        }

        [[nodiscard]] int updateCount() const
        {
            return m_updateCount;
        }

        [[nodiscard]] std::uint64_t lastFrameIndex() const
        {
            return m_lastFrameIndex;
        }

    protected:
        void onUpdate(const GameCore::Core::FrameContext& context) override
        {
            ++m_updateCount;
            m_lastFrameIndex = context.frameIndex;
            m_application.stop();
        }

    private:
        GameCore::Core::Application& m_application;
        int m_updateCount{0};
        std::uint64_t m_lastFrameIndex{0};
    };

    class ShutdownObservedScene final : public GameCore::Core::Scene
    {
    public:
        explicit ShutdownObservedScene(int& shutdownCount)
            : m_shutdownCount(shutdownCount)
        {
        }

    protected:
        void onShutdown() override
        {
            ++m_shutdownCount;
        }

    private:
        int& m_shutdownCount;
    };

    class ApplicationContextObservedScene final : public GameCore::Core::Scene
    {
    public:
        [[nodiscard]] bool hadApplicationOnInitialize() const
        {
            return m_hadApplicationOnInitialize;
        }

    protected:
        void onInitialize() override
        {
            m_hadApplicationOnInitialize = application() != nullptr;
            if (auto* app = application())
            {
                app->input().setActionDown("Confirm", true);
                app->resources().loadValue<TextResource>("scene/context-resource", []() {
                    return TextResource{"context resource"};
                });
            }
        }

    private:
        bool m_hadApplicationOnInitialize{false};
    };

    void testEntityManagerRecyclesDestroyedEntities()
    {
        GameCore::Core::EntityManager entities;

        const GameCore::Core::EntityID first = entities.createEntity();
        const GameCore::Core::EntityID second = entities.createEntity();

        assert(first != GameCore::Core::InvalidEntity);
        assert(second != GameCore::Core::InvalidEntity);
        assert(entities.livingCount() == 2);

        entities.destroyEntity(first);
        assert(!entities.isAlive(first));
        assert(entities.livingCount() == 1);

        const GameCore::Core::EntityID recycled = entities.createEntity();
        assert(recycled == first);
        assert(entities.isAlive(recycled));
        assert(entities.livingCount() == 2);
    }

    void testEntityManagerIgnoresInvalidAndDuplicateDestroy()
    {
        GameCore::Core::EntityManager entities;

        const GameCore::Core::EntityID entity = entities.createEntity();
        assert(entities.livingCount() == 1);

        entities.destroyEntity(GameCore::Core::InvalidEntity);
        entities.destroyEntity(entity);
        entities.destroyEntity(entity);

        assert(entities.livingCount() == 0);
        assert(!entities.isAlive(entity));

        const GameCore::Core::EntityID recycled = entities.createEntity();
        const GameCore::Core::EntityID next = entities.createEntity();

        assert(recycled == entity);
        assert(next != entity);
        assert(entities.livingCount() == 2);
    }

    void testComponentStorageReplacesRemovesAndExposesMutableData()
    {
        using namespace GameCore;

        Core::ComponentStorage<Components::HealthComponent> health;

        health.add(1, Components::HealthComponent{10, 20});
        health.add(1, Components::HealthComponent{15, 20});

        assert(health.size() == 1);
        assert(health.has(1));
        assert(health.get(1)->currentHealth == 15);

        health.all().at(1).currentHealth = 7;
        assert(health.get(1)->currentHealth == 7);

        health.remove(1);
        assert(!health.has(1));
        assert(health.get(1) == nullptr);

        health.add(2, Components::HealthComponent{1, 1});
        health.clear();
        assert(health.size() == 0);
    }

    void testCombatAppliesDamage()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID attacker = world.createEntity();
        const Core::EntityID target = world.createEntity();

        world.addComponent(attacker, Components::AttackComponent{30});
        world.addComponent(target, Components::HealthComponent{50, 50});

        Systems::CombatSystem combat;
        combat.attack(attacker,
                      target,
                      world.storage<Components::AttackComponent>(),
                      world.storage<Components::HealthComponent>());

        const auto* targetHealth = world.getComponent<Components::HealthComponent>(target);
        assert(targetHealth != nullptr);
        assert(targetHealth->currentHealth == 20);
    }

    void testCombatHandlesMissingComponentsDefeatAndNegativeDamage()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID attacker = world.createEntity();
        const Core::EntityID harmlessAttacker = world.createEntity();
        const Core::EntityID target = world.createEntity();
        const Core::EntityID missingHealthTarget = world.createEntity();

        world.addComponent(attacker, Components::AttackComponent{100});
        world.addComponent(harmlessAttacker, Components::AttackComponent{-50});
        world.addComponent(target, Components::HealthComponent{25, 25});

        Systems::CombatSystem combat;

        const std::string noAttack = combat.attack(missingHealthTarget,
                                                   target,
                                                   world.storage<Components::AttackComponent>(),
                                                   world.storage<Components::HealthComponent>());
        assert(noAttack == "Entity 4 cannot attack.");

        const std::string noHealth = combat.attack(attacker,
                                                   missingHealthTarget,
                                                   world.storage<Components::AttackComponent>(),
                                                   world.storage<Components::HealthComponent>());
        assert(noHealth == "Entity 4 has no health component.");

        const std::string harmless = combat.attack(harmlessAttacker,
                                                   target,
                                                   world.storage<Components::AttackComponent>(),
                                                   world.storage<Components::HealthComponent>());
        assert(harmless == "Entity 2 attacks Entity 3 for 0 damage.");
        assert(world.getComponent<Components::HealthComponent>(target)->currentHealth == 25);

        const std::string defeated = combat.attack(attacker,
                                                   target,
                                                   world.storage<Components::AttackComponent>(),
                                                   world.storage<Components::HealthComponent>());
        assert(defeated == "Entity 1 attacks Entity 3 for 100 damage. Entity 3 is defeated.");
        assert(world.getComponent<Components::HealthComponent>(target)->currentHealth == 0);

        const std::string alreadyDefeated = combat.attack(attacker,
                                                          target,
                                                          world.storage<Components::AttackComponent>(),
                                                          world.storage<Components::HealthComponent>());
        assert(alreadyDefeated == "Entity 3 is already defeated.");
    }

    void testRenderSystemIncludesAvailableComponentsOnly()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        const Core::EntityID namelessDataEntity = world.createEntity();

        world.addComponent(entity, Components::HealthComponent{8, 10});
        world.addComponent(entity, Components::PositionComponent{3, 4});
        world.addComponent(namelessDataEntity, Components::PositionComponent{-1, 2});

        Systems::RenderSystem render;
        std::ostringstream output;

        render.renderEntity(output,
                            entity,
                            "Hero",
                            world.storage<Components::HealthComponent>(),
                            world.storage<Components::PositionComponent>());
        render.renderEntity(output,
                            namelessDataEntity,
                            "Marker",
                            world.storage<Components::HealthComponent>(),
                            world.storage<Components::PositionComponent>());
        render.renderSeparator(output);

        assert(output.str() ==
               "Hero [Entity 1] Position(3, 4) Health 8/10\n"
               "Marker [Entity 2] Position(-1, 2)\n"
               "----------------------------------------\n");
    }

    void testWorldRemovesComponentsWhenEntityIsDestroyed()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.addComponent(entity, Components::AttackComponent{5});
        world.addComponent(entity, Components::HealthComponent{10, 10});

        assert(world.hasComponent<Components::AttackComponent>(entity));
        assert(world.hasComponent<Components::HealthComponent>(entity));

        world.destroyEntity(entity);

        assert(!world.isAlive(entity));
        assert(!world.hasComponent<Components::AttackComponent>(entity));
        assert(!world.hasComponent<Components::HealthComponent>(entity));
        assert(world.getComponent<Components::HealthComponent>(entity) == nullptr);
    }

    void testWorldRejectsComponentsForDeadEntities()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.destroyEntity(entity);

        bool threw = false;
        try
        {
            world.addComponent(entity, Components::HealthComponent{10, 10});
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        assert(threw);
    }

    void testWorldRecycledEntityStartsWithoutOldComponents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.addComponent(entity, Components::HealthComponent{10, 10});
        world.addComponent(entity, Components::AttackComponent{4});
        world.destroyEntity(entity);

        const Core::EntityID recycled = world.createEntity();

        assert(recycled == entity);
        assert(world.isAlive(recycled));
        assert(!world.hasComponent<Components::HealthComponent>(recycled));
        assert(!world.hasComponent<Components::AttackComponent>(recycled));
    }

    void testSystemSchedulerRunsSystemsInOrder()
    {
        using namespace GameCore;

        Core::World world;
        Core::SystemScheduler scheduler;
        std::vector<int> updateOrder;

        auto& first = scheduler.addSystem<RecordingSystem>(1, updateOrder);
        auto& second = scheduler.addSystem<RecordingSystem>(2, updateOrder);

        assert(scheduler.systemCount() == 2);

        scheduler.update(world, Core::FrameContext{0.25F, 7});

        assert((updateOrder == std::vector<int>{1, 2}));
        assert(first.lastDeltaSeconds() == 0.25F);
        assert(second.lastFrameIndex() == 7);
    }

    void testSystemSchedulerCanMutateWorld()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, Components::HealthComponent{5, 5});

        Core::SystemScheduler scheduler;
        scheduler.addSystem<HealthDecaySystem>();

        scheduler.update(world, Core::FrameContext{1.0F, 1});

        const auto* health = world.getComponent<Components::HealthComponent>(entity);
        assert(health != nullptr);
        assert(health->currentHealth == 4);

        scheduler.clear();
        assert(scheduler.systemCount() == 0);
    }

    void testEventBusPublishesTypedEvents()
    {
        using namespace GameCore;

        Core::EventBus events;
        Core::EntityID damagedEntity = Core::InvalidEntity;
        int totalDamage = 0;

        events.subscribe<DamageEvent>([&](const DamageEvent& event) {
            damagedEntity = event.entity;
            totalDamage += event.amount;
        });

        events.publish(DamageEvent{42, 7});
        events.publish(DamageEvent{42, 5});

        assert(damagedEntity == 42);
        assert(totalDamage == 12);
        assert(events.listenerCount<DamageEvent>() == 1);
    }

    void testEventBusUnsubscribesListeners()
    {
        using namespace GameCore;

        Core::EventBus events;
        int callCount = 0;

        const Core::EventBus::ListenerID listenerID =
            events.subscribe<DamageEvent>([&](const DamageEvent&) {
                ++callCount;
            });

        events.publish(DamageEvent{1, 1});
        events.unsubscribe<DamageEvent>(listenerID);
        events.publish(DamageEvent{1, 1});

        assert(callCount == 1);
        assert(events.listenerCount<DamageEvent>() == 0);
    }

    void testEventBusSupportsUnsubscribeDuringPublish()
    {
        using namespace GameCore;

        Core::EventBus events;
        int firstCallCount = 0;
        int secondCallCount = 0;
        Core::EventBus::ListenerID firstListenerID = 0;

        firstListenerID = events.subscribe<DamageEvent>([&](const DamageEvent&) {
            ++firstCallCount;
            events.unsubscribe<DamageEvent>(firstListenerID);
        });
        events.subscribe<DamageEvent>([&](const DamageEvent&) {
            ++secondCallCount;
        });

        events.publish(DamageEvent{1, 1});
        events.publish(DamageEvent{1, 1});

        assert(firstCallCount == 1);
        assert(secondCallCount == 2);
    }

    void testEventBusClearAndRejectsEmptyListeners()
    {
        using namespace GameCore;

        Core::EventBus events;
        int damageCallCount = 0;
        int numberCallCount = 0;

        events.subscribe<DamageEvent>([&](const DamageEvent&) {
            ++damageCallCount;
        });
        events.subscribe<NumberResource>([&](const NumberResource&) {
            ++numberCallCount;
        });

        events.clear<DamageEvent>();
        events.publish(DamageEvent{1, 1});
        events.publish(NumberResource{4});

        assert(damageCallCount == 0);
        assert(numberCallCount == 1);
        assert(events.listenerCount<DamageEvent>() == 0);
        assert(events.listenerCount<NumberResource>() == 1);

        events.clearAll();
        assert(events.listenerCount<NumberResource>() == 0);

        bool threw = false;
        try
        {
            events.subscribe<DamageEvent>({});
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        assert(threw);
    }

    void testScheduledSystemsCanPublishWorldEvents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        int totalDamage = 0;

        world.events().subscribe<DamageEvent>([&](const DamageEvent& event) {
            if (event.entity == entity)
            {
                totalDamage += event.amount;
            }
        });

        Core::SystemScheduler scheduler;
        scheduler.addSystem<DamagePublishingSystem>(entity);
        scheduler.update(world, Core::FrameContext{0.016F, 1});
        scheduler.update(world, Core::FrameContext{0.016F, 2});

        assert(totalDamage == 6);
    }

    void testStateMachineRunsLifecycleAndUpdateCallbacks()
    {
        using namespace GameCore;

        Core::World world;
        Core::StateMachine<TestState> machine;
        std::vector<int> calls;
        float lastDelta = 0.0F;

        machine.addState(TestState::Idle,
                         Core::StateMachine<TestState>::State{
                             [&](Core::World&) { calls.push_back(1); },
                             [&](Core::World&) { calls.push_back(2); },
                             [&](Core::World&, const Core::FrameContext& context) {
                                 lastDelta = context.deltaSeconds;
                                 calls.push_back(3);
                             },
                         });

        machine.addState(TestState::Running,
                         Core::StateMachine<TestState>::State{
                             [&](Core::World&) { calls.push_back(4); },
                             {},
                             [&](Core::World&, const Core::FrameContext&) {
                                 calls.push_back(5);
                             },
                         });

        machine.setInitialState(TestState::Idle, world);
        machine.update(world, Core::FrameContext{0.5F, 1});

        const bool changed = machine.changeState(TestState::Running, world);
        machine.update(world, Core::FrameContext{0.25F, 2});

        assert(changed);
        assert(machine.currentState().has_value());
        assert(*machine.currentState() == TestState::Running);
        assert(lastDelta == 0.5F);
        assert((calls == std::vector<int>{1, 3, 2, 4, 5}));
    }

    void testStateMachineEnforcesTransitionRules()
    {
        using namespace GameCore;

        Core::World world;
        Core::StateMachine<TestState> machine;

        machine.addState(TestState::Idle, {});
        machine.addState(TestState::Running, {});
        machine.addState(TestState::Paused, {});

        machine.allowTransition(TestState::Idle, TestState::Running);
        machine.allowTransition(TestState::Running, TestState::Paused);

        machine.setInitialState(TestState::Idle, world);

        assert(!machine.changeState(TestState::Paused, world));
        assert(machine.currentState().has_value());
        assert(*machine.currentState() == TestState::Idle);

        assert(machine.changeState(TestState::Running, world));
        assert(machine.changeState(TestState::Paused, world));
        assert(!machine.changeState(TestState::Idle, world));
    }

    void testStateMachineRejectsUnknownStatesAndClears()
    {
        using namespace GameCore;

        Core::World world;
        Core::StateMachine<TestState> machine;

        bool threw = false;
        try
        {
            machine.setInitialState(TestState::Idle, world);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        assert(threw);

        machine.addState(TestState::Idle, {});
        machine.setInitialState(TestState::Idle, world);
        assert(machine.stateCount() == 1);

        machine.clear();

        assert(machine.stateCount() == 0);
        assert(!machine.currentState().has_value());
    }

    void testStateMachineAllowsUnrestrictedTransitionsWithoutRules()
    {
        using namespace GameCore;

        Core::World world;
        Core::StateMachine<TestState> machine;

        machine.addState(TestState::Idle, {});
        machine.addState(TestState::Running, {});
        machine.addState(TestState::Paused, {});

        machine.setInitialState(TestState::Idle, world);

        assert(machine.changeState(TestState::Paused, world));
        assert(machine.changeState(TestState::Running, world));
        assert(machine.canTransitionTo(TestState::Idle));
    }

    void testSceneLifecycleIsIdempotent()
    {
        LifecycleScene scene;

        assert(!scene.isInitialized());

        scene.initialize();
        scene.initialize();

        assert(scene.isInitialized());
        assert(scene.initializeCount() == 1);

        scene.shutdown();
        scene.shutdown();

        assert(!scene.isInitialized());
        assert(scene.shutdownCount() == 1);
    }

    void testSceneUpdateInitializesAndRunsSystems()
    {
        using namespace GameCore;

        LifecycleScene scene;
        std::vector<int> updateOrder;

        scene.systems().addSystem<RecordingSystem>(1, updateOrder);
        scene.systems().addSystem<RecordingSystem>(2, updateOrder);

        scene.update(Core::FrameContext{0.016F, 3});

        assert(scene.isInitialized());
        assert(scene.initializeCount() == 1);
        assert(scene.updateCount() == 1);
        assert((updateOrder == std::vector<int>{1, 2}));
    }

    void testSceneShutdownClearsRuntimeOwnedSystemsAndEvents()
    {
        using namespace GameCore;

        LifecycleScene scene;
        std::vector<int> updateOrder;
        scene.systems().addSystem<RecordingSystem>(1, updateOrder);
        scene.world().events().subscribe<DamageEvent>([](const DamageEvent&) {});

        assert(scene.systems().systemCount() == 1);
        assert(scene.world().events().listenerCount<DamageEvent>() == 1);

        scene.initialize();
        scene.shutdown();

        assert(scene.systems().systemCount() == 0);
        assert(scene.world().events().listenerCount<DamageEvent>() == 0);
    }

    void testApplicationRunsActiveSceneForRequestedFrames()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<LifecycleScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.runFrames(3, 0.016F);

        assert(application.activeScene() == sceneView);
        assert(!application.isRunning());
        assert(application.frameIndex() == 3);
        assert(sceneView->initializeCount() == 1);
        assert(sceneView->updateCount() == 3);
    }

    void testApplicationRunWithoutSceneDoesNothing()
    {
        GameCore::Core::Application application;

        application.runFrames(5, 0.016F);

        assert(application.activeScene() == nullptr);
        assert(!application.isRunning());
        assert(application.frameIndex() == 0);
    }

    void testApplicationStopEndsFrameLoop()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<StoppingScene>(application);
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.runFrames(5, 0.016F);

        assert(!application.isRunning());
        assert(application.frameIndex() == 1);
        assert(sceneView->updateCount() == 1);
        assert(sceneView->lastFrameIndex() == 0);
    }

    void testApplicationShutsDownPreviousSceneWhenReplacing()
    {
        using namespace GameCore;

        Core::Application application;
        int firstSceneShutdownCount = 0;

        application.setScene(std::make_unique<ShutdownObservedScene>(firstSceneShutdownCount));
        application.runFrames(1, 0.016F);

        application.setScene(std::make_unique<LifecycleScene>());

        assert(firstSceneShutdownCount == 1);
        assert(application.activeScene() != nullptr);
    }

    void testApplicationCanClearActiveScene()
    {
        using namespace GameCore;

        Core::Application application;
        int shutdownCount = 0;

        application.setScene(std::make_unique<ShutdownObservedScene>(shutdownCount));
        application.runFrames(1, 0.016F);
        assert(application.activeScene() != nullptr);

        application.setScene(nullptr);

        assert(application.activeScene() == nullptr);
        assert(shutdownCount == 1);
    }

    void testSceneCanAccessApplicationContext()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<ApplicationContextObservedScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.runFrames(1, 0.016F);

        assert(sceneView->hadApplicationOnInitialize());
        assert(application.input().isHeld("Confirm"));
        assert(application.resources().has<TextResource>("scene/context-resource"));
    }

    void testInputManagerTracksPressedHeldAndReleasedStates()
    {
        GameCore::Core::InputManager input;

        assert(!input.hasAction("Attack"));
        assert(!input.wasPressed("Attack"));
        assert(!input.isHeld("Attack"));
        assert(!input.wasReleased("Attack"));

        input.setActionDown("Attack", true);

        assert(input.hasAction("Attack"));
        assert(input.wasPressed("Attack"));
        assert(input.isHeld("Attack"));
        assert(!input.wasReleased("Attack"));

        input.beginFrame();

        assert(!input.wasPressed("Attack"));
        assert(input.isHeld("Attack"));
        assert(!input.wasReleased("Attack"));

        input.setActionDown("Attack", false);

        assert(!input.wasPressed("Attack"));
        assert(!input.isHeld("Attack"));
        assert(input.wasReleased("Attack"));

        input.beginFrame();

        assert(!input.wasPressed("Attack"));
        assert(!input.isHeld("Attack"));
        assert(!input.wasReleased("Attack"));
    }

    void testInputManagerRejectsEmptyActionsAndClears()
    {
        GameCore::Core::InputManager input;
        input.setActionDown("Confirm", true);
        input.setActionDown("Cancel", false);

        assert(input.actionCount() == 2);
        assert(input.hasAction("Confirm"));
        assert(input.hasAction("Cancel"));

        bool threw = false;
        try
        {
            input.setActionDown("", true);
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        input.clear();

        assert(threw);
        assert(input.actionCount() == 0);
        assert(!input.hasAction("Confirm"));
    }

    void testApplicationAdvancesInputFramesAfterSceneUpdate()
    {
        using namespace GameCore;

        Core::Application application;
        application.input().setActionDown("Attack", true);

        assert(application.input().wasPressed("Attack"));

        application.setScene(std::make_unique<LifecycleScene>());
        application.runFrames(1, 0.016F);

        assert(application.input().isHeld("Attack"));
        assert(!application.input().wasPressed("Attack"));
    }

    void testSceneManagerRegistersAndCreatesScenes()
    {
        using namespace GameCore;

        Core::SceneManager scenes;
        scenes.registerScene("test", []() {
            return std::make_unique<LifecycleScene>();
        });

        assert(scenes.hasScene("test"));
        assert(scenes.sceneCount() == 1);

        auto scene = scenes.createScene("test");
        assert(scene != nullptr);

        scenes.unregisterScene("test");
        assert(!scenes.hasScene("test"));
        assert(scenes.sceneCount() == 0);
        assert(scenes.createScene("test") == nullptr);
    }

    void testSceneManagerChangesApplicationScene()
    {
        using namespace GameCore;

        Core::Application application;
        Core::SceneManager scenes;

        scenes.registerScene("menu", []() {
            return std::make_unique<LifecycleScene>();
        });

        const bool changed = scenes.changeScene(application, "menu");

        assert(changed);
        assert(application.activeScene() != nullptr);

        application.runFrames(1, 0.016F);
        assert(application.frameIndex() == 1);
    }

    void testSceneManagerRejectsMissingScenesAndEmptyFactories()
    {
        using namespace GameCore;

        Core::Application application;
        Core::SceneManager scenes;

        assert(!scenes.changeScene(application, "missing"));
        assert(application.activeScene() == nullptr);

        bool threw = false;
        try
        {
            scenes.registerScene("invalid", {});
        }
        catch (const std::invalid_argument&)
        {
            threw = true;
        }

        assert(threw);
    }

    void testResourceManagerLoadsAndCachesTypedResources()
    {
        using namespace GameCore;

        Core::ResourceManager resources;
        int loadCount = 0;

        auto first = resources.load<TextResource>("dialogue/start", [&]() {
            ++loadCount;
            return std::make_shared<TextResource>(TextResource{"hello"});
        });

        auto second = resources.load<TextResource>("dialogue/start", [&]() {
            ++loadCount;
            return std::make_shared<TextResource>(TextResource{"ignored"});
        });

        assert(first.isValid());
        assert(second.isValid());
        assert(first.id() == "dialogue/start");
        assert(first.get() == second.get());
        assert(first->text == "hello");
        assert(loadCount == 1);
        assert(resources.resourceCount<TextResource>() == 1);
        assert(resources.resourceTypeCount() == 1);
    }

    void testResourceManagerKeepsResourceTypesSeparate()
    {
        using namespace GameCore;

        Core::ResourceManager resources;

        resources.loadValue<TextResource>("shared/id", []() {
            return TextResource{"text"};
        });

        resources.loadValue<NumberResource>("shared/id", []() {
            return NumberResource{42};
        });

        assert(resources.has<TextResource>("shared/id"));
        assert(resources.has<NumberResource>("shared/id"));
        assert(resources.require<TextResource>("shared/id")->text == "text");
        assert(resources.require<NumberResource>("shared/id")->value == 42);
        assert(resources.resourceTypeCount() == 2);
    }

    void testResourceManagerUnloadsAndClearsResources()
    {
        using namespace GameCore;

        Core::ResourceManager resources;
        auto handle = resources.loadValue<TextResource>("ui/title", []() {
            return TextResource{"title"};
        });

        assert(handle.isValid());
        assert(resources.has<TextResource>("ui/title"));

        resources.unload<TextResource>("ui/title");

        assert(!resources.has<TextResource>("ui/title"));
        assert(handle.isValid());
        assert(handle->text == "title");

        resources.loadValue<TextResource>("ui/title", []() {
            return TextResource{"new title"};
        });
        resources.loadValue<NumberResource>("numbers/one", []() {
            return NumberResource{1};
        });

        resources.clearAll();

        assert(resources.resourceTypeCount() == 0);
        assert(!resources.has<TextResource>("ui/title"));
        assert(!resources.has<NumberResource>("numbers/one"));
    }

    void testResourceManagerReloadsUsingStoredLoader()
    {
        using namespace GameCore;

        Core::ResourceManager resources;
        int version = 0;

        auto oldHandle = resources.load<TextResource>("config/main", [&]() {
            ++version;
            return std::make_shared<TextResource>(TextResource{"version " + std::to_string(version)});
        });

        assert(oldHandle->text == "version 1");

        const bool reloaded = resources.reload<TextResource>("config/main");
        auto newHandle = resources.require<TextResource>("config/main");

        assert(reloaded);
        assert(oldHandle->text == "version 1");
        assert(newHandle->text == "version 2");
        assert(oldHandle.get() != newHandle.get());
    }

    void testResourceManagerRejectsInvalidLoads()
    {
        using namespace GameCore;

        Core::ResourceManager resources;

        bool emptyIDThrew = false;
        try
        {
            resources.loadValue<TextResource>("", []() {
                return TextResource{"bad"};
            });
        }
        catch (const std::invalid_argument&)
        {
            emptyIDThrew = true;
        }

        bool emptyLoaderThrew = false;
        try
        {
            resources.load<TextResource>("bad/empty-loader", {});
        }
        catch (const std::invalid_argument&)
        {
            emptyLoaderThrew = true;
        }

        bool nullLoaderThrew = false;
        try
        {
            resources.load<TextResource>("bad/null-loader", []() {
                return std::shared_ptr<TextResource>{};
            });
        }
        catch (const std::runtime_error&)
        {
            nullLoaderThrew = true;
        }

        assert(emptyIDThrew);
        assert(emptyLoaderThrew);
        assert(nullLoaderThrew);
    }

    void testResourceManagerRequireMissingAndReloadMissing()
    {
        using namespace GameCore;

        Core::ResourceManager resources;

        assert(!resources.has<TextResource>("missing"));
        assert(!resources.get<TextResource>("missing").isValid());
        assert(!resources.reload<TextResource>("missing"));

        bool threw = false;
        try
        {
            [[maybe_unused]] const auto resource = resources.require<TextResource>("missing");
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        assert(threw);
    }

    void testResourceManagerReloadFailureKeepsExistingResource()
    {
        using namespace GameCore;

        Core::ResourceManager resources;
        bool shouldFail = false;

        auto handle = resources.load<TextResource>("config/reload-failure", [&]() {
            if (shouldFail)
            {
                throw std::runtime_error("reload failed");
            }

            return std::make_shared<TextResource>(TextResource{"stable"});
        });

        shouldFail = true;

        bool threw = false;
        try
        {
            resources.reload<TextResource>("config/reload-failure");
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        const auto afterFailure = resources.require<TextResource>("config/reload-failure");

        assert(threw);
        assert(handle.get() == afterFailure.get());
        assert(afterFailure->text == "stable");
    }

    void testApplicationResourcesSurviveSceneChanges()
    {
        using namespace GameCore;

        Core::Application application;
        Core::SceneManager scenes;
        scenes.registerScene("first", []() {
            return std::make_unique<LifecycleScene>();
        });
        scenes.registerScene("second", []() {
            return std::make_unique<LifecycleScene>();
        });

        application.resources().loadValue<TextResource>("shared/font", []() {
            return TextResource{"font data"};
        });

        assert(scenes.changeScene(application, "first"));
        assert(scenes.changeScene(application, "second"));
        assert(application.resources().has<TextResource>("shared/font"));
        assert(application.resources().require<TextResource>("shared/font")->text == "font data");
    }

    void testFileSystemReadsTextAndBinaryFiles()
    {
        using namespace GameCore;

        const auto textPath = testAssetPath("gamecore_text_asset.txt");
        const auto binaryPath = testAssetPath("gamecore_binary_asset.bin");

        writeTextFile(textPath, "line one\nline two");
        writeBinaryFile(binaryPath, std::vector<std::uint8_t>{0, 1, 2, 255});

        assert(Core::FileSystem::exists(textPath.string()));
        assert(Core::FileSystem::readTextFile(textPath.string()) == "line one\nline two");

        const auto bytes = Core::FileSystem::readBinaryFile(binaryPath.string());
        assert((bytes == std::vector<std::uint8_t>{0, 1, 2, 255}));

        std::filesystem::remove(textPath);
        std::filesystem::remove(binaryPath);
    }

    void testFileSystemReadsEmptyFiles()
    {
        using namespace GameCore;

        const auto textPath = testAssetPath("gamecore_empty_text_asset.txt");
        const auto binaryPath = testAssetPath("gamecore_empty_binary_asset.bin");

        writeTextFile(textPath, "");
        writeBinaryFile(binaryPath, {});

        assert(Core::FileSystem::exists(textPath.string()));
        assert(Core::FileSystem::readTextFile(textPath.string()).empty());
        assert(Core::FileSystem::readBinaryFile(binaryPath.string()).empty());

        std::filesystem::remove(textPath);
        std::filesystem::remove(binaryPath);
    }

    void testFileSystemThrowsForMissingFiles()
    {
        using namespace GameCore;

        const auto path = testAssetPath("gamecore_missing_asset.txt");
        std::filesystem::remove(path);

        bool threw = false;
        try
        {
            [[maybe_unused]] const auto text = Core::FileSystem::readTextFile(path.string());
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        assert(!Core::FileSystem::exists(path.string()));
        assert(threw);
    }

    void testFileSystemThrowsForMissingBinaryFiles()
    {
        using namespace GameCore;

        const auto path = testAssetPath("gamecore_missing_asset.bin");
        std::filesystem::remove(path);

        bool threw = false;
        try
        {
            [[maybe_unused]] const auto bytes = Core::FileSystem::readBinaryFile(path.string());
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        assert(!Core::FileSystem::exists(path.string()));
        assert(threw);
    }

    void testAssetLoadersLoadTextAndBinaryResources()
    {
        using namespace GameCore;

        const auto textPath = testAssetPath("gamecore_loader_text_asset.txt");
        const auto binaryPath = testAssetPath("gamecore_loader_binary_asset.bin");

        writeTextFile(textPath, "asset text");
        writeBinaryFile(binaryPath, std::vector<std::uint8_t>{10, 20, 30});

        Core::ResourceManager resources;
        const auto text = Core::AssetLoaders::loadText(resources, "text/example", textPath.string());
        const auto binary = Core::AssetLoaders::loadBinary(resources, "binary/example", binaryPath.string());

        assert(text.isValid());
        assert(text->text == "asset text");
        assert(text->sourcePath == textPath.string());

        assert(binary.isValid());
        assert((binary->bytes == std::vector<std::uint8_t>{10, 20, 30}));
        assert(binary->sourcePath == binaryPath.string());

        std::filesystem::remove(textPath);
        std::filesystem::remove(binaryPath);
    }

    void testAssetLoaderReloadReadsChangedFile()
    {
        using namespace GameCore;

        const auto path = testAssetPath("gamecore_reload_text_asset.txt");
        writeTextFile(path, "first");

        Core::ResourceManager resources;
        const auto oldHandle = Core::AssetLoaders::loadText(resources, "text/reload", path.string());
        assert(oldHandle->text == "first");

        writeTextFile(path, "second");
        const bool reloaded = resources.reload<Core::TextResource>("text/reload");
        const auto newHandle = resources.require<Core::TextResource>("text/reload");

        assert(reloaded);
        assert(oldHandle->text == "first");
        assert(newHandle->text == "second");
        assert(oldHandle.get() != newHandle.get());

        std::filesystem::remove(path);
    }

    void testAssetLoaderMissingFileDoesNotCacheResource()
    {
        using namespace GameCore;

        const auto path = testAssetPath("gamecore_missing_loader_text_asset.txt");
        std::filesystem::remove(path);

        Core::ResourceManager resources;
        bool threw = false;
        try
        {
            [[maybe_unused]] const auto text =
                Core::AssetLoaders::loadText(resources, "text/missing", path.string());
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }

        assert(threw);
        assert(!resources.has<Core::TextResource>("text/missing"));
        assert(resources.resourceCount<Core::TextResource>() == 0);
    }

    void testKeyValuePrefabLoaderParsesMultipleEntities()
    {
        using namespace GameCore;

        const std::string text =
            "# combat demo\n"
            "[Player]\n"
            "health.current=120\n"
            "health.max=120\n"
            "attack.damage=25\n"
            "position.x=0\n"
            "position.y=0\n"
            "\n"
            "[Enemy]\n"
            "health.current=90\n"
            "health.max=90\n"
            "attack.damage=15\n"
            "position.x=1\n"
            "position.y=0\n";

        const Core::PrefabDocument document = Core::KeyValuePrefabLoader::loadFromText(text);

        assert(document.entities.size() == 2);
        assert(document.entities[0].name == "Player");
        assert(document.entities[0].health->currentHealth == 120);
        assert(document.entities[0].health->maxHealth == 120);
        assert(document.entities[0].attack->damage == 25);
        assert(document.entities[0].position->x == 0);
        assert(document.entities[0].position->y == 0);

        assert(document.entities[1].name == "Enemy");
        assert(document.entities[1].health->currentHealth == 90);
        assert(document.entities[1].health->maxHealth == 90);
        assert(document.entities[1].attack->damage == 15);
        assert(document.entities[1].position->x == 1);
        assert(document.entities[1].position->y == 0);
    }

    void testPrefabInstantiatorCreatesEntitiesWithComponents()
    {
        using namespace GameCore;

        Core::PrefabDocument document;
        document.entities.push_back(Core::EntityPrefab{
            "Player",
            Components::HealthComponent{120, 120},
            Components::AttackComponent{25},
            Components::PositionComponent{0, 0},
        });
        document.entities.push_back(Core::EntityPrefab{
            "Marker",
            std::nullopt,
            std::nullopt,
            Components::PositionComponent{5, 6},
        });

        Core::World world;
        const auto entities = Core::PrefabInstantiator::instantiateAll(world, document);

        assert(entities.size() == 2);
        assert(world.livingCount() == 2);

        assert(world.getComponent<Components::HealthComponent>(entities[0])->currentHealth == 120);
        assert(world.getComponent<Components::AttackComponent>(entities[0])->damage == 25);
        assert(world.getComponent<Components::PositionComponent>(entities[0])->x == 0);

        assert(!world.hasComponent<Components::HealthComponent>(entities[1]));
        assert(!world.hasComponent<Components::AttackComponent>(entities[1]));
        assert(world.getComponent<Components::PositionComponent>(entities[1])->x == 5);
        assert(world.getComponent<Components::PositionComponent>(entities[1])->y == 6);
    }

    void testKeyValuePrefabLoaderReportsInvalidInput()
    {
        using namespace GameCore;

        bool propertyBeforeSectionThrew = false;
        try
        {
            Core::KeyValuePrefabLoader::loadFromText("health.current=10\n");
        }
        catch (const std::runtime_error&)
        {
            propertyBeforeSectionThrew = true;
        }

        bool unknownKeyThrew = false;
        try
        {
            Core::KeyValuePrefabLoader::loadFromText("[Entity]\nunknown.value=1\n");
        }
        catch (const std::runtime_error&)
        {
            unknownKeyThrew = true;
        }

        bool invalidIntegerThrew = false;
        try
        {
            Core::KeyValuePrefabLoader::loadFromText("[Entity]\nhealth.current=abc\n");
        }
        catch (const std::runtime_error&)
        {
            invalidIntegerThrew = true;
        }

        assert(propertyBeforeSectionThrew);
        assert(unknownKeyThrew);
        assert(invalidIntegerThrew);
    }

    void testPrefabAssetLoaderLoadsAndReloadsDocuments()
    {
        using namespace GameCore;

        const auto path = testAssetPath("combat.prefab");
        writeTextFile(path,
                      "[Player]\n"
                      "health.current=120\n"
                      "health.max=120\n"
                      "attack.damage=25\n");

        Core::ResourceManager resources;
        const auto oldHandle = Core::AssetLoaders::loadKeyValuePrefab(resources,
                                                                      "prefabs/combat",
                                                                      path.string());

        assert(oldHandle.isValid());
        assert(oldHandle->entities.size() == 1);
        assert(oldHandle->entities[0].attack->damage == 25);

        writeTextFile(path,
                      "[Player]\n"
                      "health.current=120\n"
                      "health.max=120\n"
                      "attack.damage=30\n"
                      "\n"
                      "[Enemy]\n"
                      "health.current=90\n"
                      "health.max=90\n");

        const bool reloaded = resources.reload<Core::PrefabDocument>("prefabs/combat");
        const auto newHandle = resources.require<Core::PrefabDocument>("prefabs/combat");

        assert(reloaded);
        assert(oldHandle->entities.size() == 1);
        assert(newHandle->entities.size() == 2);
        assert(newHandle->entities[0].attack->damage == 30);
        assert(newHandle->entities[1].name == "Enemy");

        std::filesystem::remove(path);
    }
}

int main()
{
    testEntityManagerRecyclesDestroyedEntities();
    testEntityManagerIgnoresInvalidAndDuplicateDestroy();
    testComponentStorageReplacesRemovesAndExposesMutableData();
    testCombatAppliesDamage();
    testCombatHandlesMissingComponentsDefeatAndNegativeDamage();
    testRenderSystemIncludesAvailableComponentsOnly();
    testWorldRemovesComponentsWhenEntityIsDestroyed();
    testWorldRejectsComponentsForDeadEntities();
    testWorldRecycledEntityStartsWithoutOldComponents();
    testSystemSchedulerRunsSystemsInOrder();
    testSystemSchedulerCanMutateWorld();
    testEventBusPublishesTypedEvents();
    testEventBusUnsubscribesListeners();
    testEventBusSupportsUnsubscribeDuringPublish();
    testEventBusClearAndRejectsEmptyListeners();
    testScheduledSystemsCanPublishWorldEvents();
    testStateMachineRunsLifecycleAndUpdateCallbacks();
    testStateMachineEnforcesTransitionRules();
    testStateMachineRejectsUnknownStatesAndClears();
    testStateMachineAllowsUnrestrictedTransitionsWithoutRules();
    testSceneLifecycleIsIdempotent();
    testSceneUpdateInitializesAndRunsSystems();
    testSceneShutdownClearsRuntimeOwnedSystemsAndEvents();
    testApplicationRunsActiveSceneForRequestedFrames();
    testApplicationRunWithoutSceneDoesNothing();
    testApplicationStopEndsFrameLoop();
    testApplicationShutsDownPreviousSceneWhenReplacing();
    testApplicationCanClearActiveScene();
    testSceneCanAccessApplicationContext();
    testInputManagerTracksPressedHeldAndReleasedStates();
    testInputManagerRejectsEmptyActionsAndClears();
    testApplicationAdvancesInputFramesAfterSceneUpdate();
    testSceneManagerRegistersAndCreatesScenes();
    testSceneManagerChangesApplicationScene();
    testSceneManagerRejectsMissingScenesAndEmptyFactories();
    testResourceManagerLoadsAndCachesTypedResources();
    testResourceManagerKeepsResourceTypesSeparate();
    testResourceManagerUnloadsAndClearsResources();
    testResourceManagerReloadsUsingStoredLoader();
    testResourceManagerRejectsInvalidLoads();
    testResourceManagerRequireMissingAndReloadMissing();
    testResourceManagerReloadFailureKeepsExistingResource();
    testApplicationResourcesSurviveSceneChanges();
    testFileSystemReadsTextAndBinaryFiles();
    testFileSystemReadsEmptyFiles();
    testFileSystemThrowsForMissingFiles();
    testFileSystemThrowsForMissingBinaryFiles();
    testAssetLoadersLoadTextAndBinaryResources();
    testAssetLoaderReloadReadsChangedFile();
    testAssetLoaderMissingFileDoesNotCacheResource();
    testKeyValuePrefabLoaderParsesMultipleEntities();
    testPrefabInstantiatorCreatesEntitiesWithComponents();
    testKeyValuePrefabLoaderReportsInvalidInput();
    testPrefabAssetLoaderLoadsAndReloadsDocuments();
    return 0;
}
