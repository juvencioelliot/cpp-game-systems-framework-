#include "GameCore/Components/MoveIntentComponent.h"
#include "GameCore/Components/NameComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Components/ProgressComponent.h"
#include "GameCore/Components/RenderableComponent.h"
#include "GameCore/Components/TransformComponent.h"
#include "GameCore/Core/Application.h"
#include "GameCore/Core/AssetLoaders.h"
#include "GameCore/Core/ComponentStorage.h"
#include "GameCore/Core/Diagnostics.h"
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
#include "GameCore/Systems/MovementSystem.h"
#include "GameCore/Systems/RenderSystem.h"
#include "GameCore/Systems/TransformSystem.h"

#include <algorithm>
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

    struct SpeedComponent
    {
        int value{0};
    };

    struct TestValueComponent
    {
        int value{0};
        int maxValue{0};
    };

    struct TestDeltaComponent
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
            m_lastTotalSeconds = context.totalSeconds;
            m_lastFixedFrameIndex = context.fixedFrameIndex;
        }

        [[nodiscard]] float lastDeltaSeconds() const
        {
            return m_lastDeltaSeconds;
        }

        [[nodiscard]] std::uint64_t lastFrameIndex() const
        {
            return m_lastFrameIndex;
        }

        [[nodiscard]] float lastTotalSeconds() const
        {
            return m_lastTotalSeconds;
        }

        [[nodiscard]] std::uint64_t lastFixedFrameIndex() const
        {
            return m_lastFixedFrameIndex;
        }

    private:
        int m_id{0};
        std::vector<int>& m_updateOrder;
        float m_lastDeltaSeconds{0.0F};
        std::uint64_t m_lastFrameIndex{0};
        float m_lastTotalSeconds{0.0F};
        std::uint64_t m_lastFixedFrameIndex{0};
    };

    class ValueDecaySystem final : public GameCore::Core::ISystem
    {
    public:
        void update(GameCore::Core::World& world, const GameCore::Core::FrameContext&) override
        {
            auto& values = world.storage<TestValueComponent>();
            for (auto& entry : values.all())
            {
                --entry.second.value;
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

    class DeferredDestroySystem final : public GameCore::Core::ISystem
    {
    public:
        explicit DeferredDestroySystem(GameCore::Core::EntityID entity)
            : m_entity(entity)
        {
        }

        void update(GameCore::Core::World& world, const GameCore::Core::FrameContext&) override
        {
            world.deferDestroyEntity(m_entity);
            m_wasAliveDuringUpdate = world.isAlive(m_entity);
        }

        [[nodiscard]] bool wasAliveDuringUpdate() const
        {
            return m_wasAliveDuringUpdate;
        }

    private:
        GameCore::Core::EntityID m_entity{GameCore::Core::InvalidEntity};
        bool m_wasAliveDuringUpdate{false};
    };

    class DeferredComponentMutationSystem final : public GameCore::Core::ISystem
    {
    public:
        explicit DeferredComponentMutationSystem(GameCore::Core::EntityID entity)
            : m_entity(entity)
        {
        }

        void update(GameCore::Core::World& world, const GameCore::Core::FrameContext&) override
        {
            world.deferAddComponent(m_entity, TestDeltaComponent{9});
            m_hadComponentDuringUpdate = world.hasComponent<TestDeltaComponent>(m_entity);
        }

        [[nodiscard]] bool hadComponentDuringUpdate() const
        {
            return m_hadComponentDuringUpdate;
        }

    private:
        GameCore::Core::EntityID m_entity{GameCore::Core::InvalidEntity};
        bool m_hadComponentDuringUpdate{false};
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

    class SequencedScene final : public GameCore::Core::Scene
    {
    public:
        explicit SequencedScene(std::vector<int>& order)
            : m_order(order)
        {
        }

    protected:
        void onBeforeSystems(const GameCore::Core::FrameContext&) override
        {
            m_order.push_back(1);
        }

        void onAfterSystems(const GameCore::Core::FrameContext&) override
        {
            m_order.push_back(3);
        }

    private:
        std::vector<int>& m_order;
    };

    class DeferredMutationObservedScene final : public GameCore::Core::Scene
    {
    public:
        [[nodiscard]] bool hadComponentAfterSystems() const
        {
            return m_hadComponentAfterSystems;
        }

    protected:
        void onInitialize() override
        {
            m_entity = world().createEntity();
            world().addComponent(m_entity, TestValueComponent{1, 1});
            systems().addSystem<DeferredComponentMutationSystem>(m_entity);
        }

        void onAfterSystems(const GameCore::Core::FrameContext&) override
        {
            m_hadComponentAfterSystems = world().hasComponent<TestDeltaComponent>(m_entity);
        }

    private:
        GameCore::Core::EntityID m_entity{GameCore::Core::InvalidEntity};
        bool m_hadComponentAfterSystems{false};
    };

    class BeforeSystemsMoveIntentScene final : public GameCore::Core::Scene
    {
    public:
        [[nodiscard]] int entityX() const
        {
            const auto* position =
                world().getComponent<GameCore::Components::PositionComponent>(m_entity);
            return position == nullptr ? -1 : position->x;
        }

    protected:
        void onInitialize() override
        {
            m_entity = world().createEntity();
            world().addComponent(m_entity, GameCore::Components::PositionComponent{0, 0});
            world().addComponent(m_entity, GameCore::Components::MoveIntentComponent{});
            systems().addSystem<GameCore::Systems::MovementSystem>(0, 0, 10, 10);
        }

        void onBeforeSystems(const GameCore::Core::FrameContext&) override
        {
            auto* intent =
                world().getComponent<GameCore::Components::MoveIntentComponent>(m_entity);
            if (intent != nullptr)
            {
                intent->dx = 1;
            }
        }

    private:
        GameCore::Core::EntityID m_entity{GameCore::Core::InvalidEntity};
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

    class ContextRecordingScene final : public GameCore::Core::Scene
    {
    public:
        [[nodiscard]] const std::vector<GameCore::Core::FrameContext>& contexts() const
        {
            return m_contexts;
        }

    protected:
        void onUpdate(const GameCore::Core::FrameContext& context) override
        {
            m_contexts.push_back(context);
        }

    private:
        std::vector<GameCore::Core::FrameContext> m_contexts;
    };

    class SequenceClock final : public GameCore::Core::IClock
    {
    public:
        explicit SequenceClock(std::vector<double> times)
            : m_times(std::move(times))
        {
        }

        [[nodiscard]] double nowSeconds() const override
        {
            if (m_times.empty())
            {
                return 0.0;
            }

            const auto index = std::min(m_nextTimeIndex, m_times.size() - 1);
            ++m_nextTimeIndex;
            return m_times[index];
        }

    private:
        std::vector<double> m_times;
        mutable std::size_t m_nextTimeIndex{0};
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
        assert(GameCore::Core::entityIndex(recycled) == GameCore::Core::entityIndex(first));
        assert(recycled != first);
        assert(!entities.isAlive(first));
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

        assert(GameCore::Core::entityIndex(recycled) == GameCore::Core::entityIndex(entity));
        assert(recycled != entity);
        assert(next != entity);
        assert(entities.livingCount() == 2);
    }

    void testComponentStorageReplacesRemovesAndExposesMutableData()
    {
        using namespace GameCore;

        Core::ComponentStorage<TestValueComponent> values;

        values.add(1, TestValueComponent{10, 20});
        values.add(1, TestValueComponent{15, 20});

        assert(values.size() == 1);
        assert(values.has(1));
        assert(values.get(1)->value == 15);

        values.all().at(1).value = 7;
        assert(values.get(1)->value == 7);

        values.remove(1);
        assert(!values.has(1));
        assert(values.get(1) == nullptr);

        values.add(2, TestValueComponent{1, 1});
        values.clear();
        assert(values.size() == 0);
    }

    void testRenderSystemIncludesAvailableComponentsOnly()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        const Core::EntityID namelessDataEntity = world.createEntity();

        world.addComponent(entity, Components::ProgressComponent{8, 10});
        world.addComponent(entity, Components::PositionComponent{3, 4});
        world.addComponent(namelessDataEntity, Components::PositionComponent{-1, 2});

        std::ostringstream output;
        Systems::RenderSystem render(
            std::make_unique<Systems::TerminalRenderBackend>(output, false),
            std::vector<Systems::RenderEntityLabel>{
                Systems::RenderEntityLabel{entity, "Hero", 'H'},
                Systems::RenderEntityLabel{namelessDataEntity, "Marker", 'M'},
            });

        render.update(world, Core::FrameContext{0.016F, 9});

        const std::string renderedFrame = output.str();
        assert(renderedFrame.find("GameCoreCPP Runtime Renderer | frame 9") != std::string::npos);
        assert(renderedFrame.find("H Hero Progress 8/10") != std::string::npos);
        assert(renderedFrame.find("M Marker") != std::string::npos);
        assert(!render.shouldClose());
    }

    void testRenderSystemBuildsLayeredDrawCommands()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        const Core::EntityID marker = world.createEntity();

        world.addComponent(entity, Components::ProgressComponent{8, 10});
        world.addComponent(entity, Components::PositionComponent{3, 4});
        world.addComponent(marker, Components::PositionComponent{2, 1});

        const Systems::DrawFrame frame = Systems::buildDrawFrame(
            world,
            Core::FrameContext{0.016F, 12},
            std::vector<Systems::RenderEntityLabel>{
                Systems::RenderEntityLabel{entity, "Hero", 'H'},
                Systems::RenderEntityLabel{marker, "Marker", 'M'},
            });

        assert(frame.frameIndex == 12);
        assert(frame.deltaSeconds == 0.016F);
        assert(frame.commands.size() == 4);

        assert(frame.commands[0].type == Systems::DrawCommandType::GridCell);
        assert(frame.commands[0].glyph == 'H');
        assert(frame.commands[0].x == 3);
        assert(frame.commands[0].y == 4);

        assert(frame.commands[1].type == Systems::DrawCommandType::GridCell);
        assert(frame.commands[1].glyph == 'M');
        assert(frame.commands[1].x == 2);
        assert(frame.commands[1].y == 1);

        assert(frame.commands[2].type == Systems::DrawCommandType::ProgressBar);
        assert(frame.commands[2].glyph == 'H');
        assert(frame.commands[2].text == "Hero");
        assert(frame.commands[2].value == 8);
        assert(frame.commands[2].maxValue == 10);
        assert(frame.commands[2].layer >= frame.commands[1].layer);

        assert(frame.commands[3].type == Systems::DrawCommandType::Text);
        assert(frame.commands[3].glyph == 'M');
        assert(frame.commands[3].text == "Marker");
        assert(frame.commands[3].layer >= frame.commands[2].layer);
    }

    void testRenderSystemBuildsDrawCommandsFromRenderableComponents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, Components::NameComponent{"Actor"});
        world.addComponent(entity, Components::PositionComponent{7, 2});
        world.addComponent(entity, Components::ProgressComponent{3, 5});
        world.addComponent(entity, Components::RenderableComponent{'A', 4, true});

        const Systems::DrawFrame frame =
            Systems::buildDrawFrame(world, Core::FrameContext{0.016F, 33});

        assert(frame.frameIndex == 33);
        assert(frame.commands.size() == 2);
        assert(frame.commands[0].type == Systems::DrawCommandType::GridCell);
        assert(frame.commands[0].glyph == 'A');
        assert(frame.commands[0].text == "Actor");
        assert(frame.commands[0].x == 7);
        assert(frame.commands[0].y == 2);
        assert(frame.commands[1].type == Systems::DrawCommandType::ProgressBar);
        assert(frame.commands[1].text == "Actor");
        assert(frame.commands[1].value == 3);
        assert(frame.commands[1].maxValue == 5);
    }

    void testRenderSystemSkipsInvisibleRenderableComponents()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, Components::PositionComponent{7, 2});
        world.addComponent(entity, Components::RenderableComponent{'A', 4, false});

        const Systems::DrawFrame frame = Systems::buildDrawFrame(world, Core::FrameContext{});

        assert(frame.commands.empty());
    }

    void testWorldRemovesComponentsWhenEntityIsDestroyed()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.addComponent(entity, TestDeltaComponent{5});
        world.addComponent(entity, TestValueComponent{10, 10});

        assert(world.hasComponent<TestDeltaComponent>(entity));
        assert(world.hasComponent<TestValueComponent>(entity));

        world.destroyEntity(entity);

        assert(!world.isAlive(entity));
        assert(!world.hasComponent<TestDeltaComponent>(entity));
        assert(!world.hasComponent<TestValueComponent>(entity));
        assert(world.getComponent<TestValueComponent>(entity) == nullptr);
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
            world.addComponent(entity, TestValueComponent{10, 10});
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

        world.addComponent(entity, TestValueComponent{10, 10});
        world.addComponent(entity, TestDeltaComponent{4});
        world.destroyEntity(entity);

        const Core::EntityID recycled = world.createEntity();

        assert(Core::entityIndex(recycled) == Core::entityIndex(entity));
        assert(recycled != entity);
        assert(world.isAlive(recycled));
        assert(!world.isAlive(entity));
        assert(!world.hasComponent<TestValueComponent>(recycled));
        assert(!world.hasComponent<TestDeltaComponent>(recycled));
    }

    void testWorldRecycledEntityWrapsGenerationSafely()
    {
        using namespace GameCore;

        Core::World world;
        Core::EntityID previous = world.createEntity();
        const auto index = Core::entityIndex(previous);

        for (std::uint32_t i = 0; i < Core::EntityMaxGeneration + 2; ++i)
        {
            world.destroyEntity(previous);
            const Core::EntityID recycled = world.createEntity();

            assert(Core::entityIndex(recycled) == index);
            assert(recycled != previous);
            assert(world.isAlive(recycled));
            assert(!world.isAlive(previous));

            previous = recycled;
        }
    }

    void testWorldFlushesDeferredDestruction()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, TestValueComponent{1, 1});

        world.deferDestroyEntity(entity);

        assert(world.isAlive(entity));
        assert(world.deferredDestroyCount() == 1);

        world.flushDeferredDestruction();

        assert(!world.isAlive(entity));
        assert(world.deferredDestroyCount() == 0);
        assert(world.getComponent<TestValueComponent>(entity) == nullptr);
    }

    void testWorldDeduplicatesDeferredDestruction()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.deferDestroyEntity(entity);
        world.deferDestroyEntity(entity);

        assert(world.deferredDestroyCount() == 1);

        world.flushDeferredDestruction();

        assert(!world.isAlive(entity));
        assert(world.deferredDestroyCount() == 0);
    }

    void testWorldFlushesDeferredComponentAddAndRemove()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, TestValueComponent{10, 10});

        world.deferAddComponent(entity, TestDeltaComponent{4});
        world.deferRemoveComponent<TestValueComponent>(entity);

        assert(world.deferredComponentMutationCount() == 2);
        assert(!world.hasComponent<TestDeltaComponent>(entity));
        assert(world.hasComponent<TestValueComponent>(entity));

        world.flushDeferredComponentMutations();

        assert(world.deferredComponentMutationCount() == 0);
        assert(world.hasComponent<TestDeltaComponent>(entity));
        assert(world.getComponent<TestDeltaComponent>(entity)->value == 4);
        assert(!world.hasComponent<TestValueComponent>(entity));
    }

    void testWorldDeferredComponentAddSkipsDestroyedEntities()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();

        world.deferAddComponent(entity, TestDeltaComponent{8});
        world.destroyEntity(entity);
        world.flushDeferredComponentMutations();

        assert(!world.isAlive(entity));
        assert(!world.hasComponent<TestDeltaComponent>(entity));
    }

    void testWorldDeferredComponentMutationIsSafeDuringIteration()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID first = world.createEntity();
        const Core::EntityID second = world.createEntity();
        world.addComponent(first, TestValueComponent{1, 1});
        world.addComponent(second, TestValueComponent{2, 2});

        std::vector<Core::EntityID> visited;
        world.each<TestValueComponent>(
            [&world, &visited](Core::EntityID entity, TestValueComponent&) {
                visited.push_back(entity);
                world.deferAddComponent(entity, TestDeltaComponent{3});
                world.deferRemoveComponent<TestValueComponent>(entity);
            });

        assert(visited.size() == 2);
        assert(world.deferredComponentMutationCount() == 4);
        assert(world.hasComponent<TestValueComponent>(first));
        assert(!world.hasComponent<TestDeltaComponent>(first));

        world.flushDeferredComponentMutations();

        assert(!world.hasComponent<TestValueComponent>(first));
        assert(!world.hasComponent<TestValueComponent>(second));
        assert(world.hasComponent<TestDeltaComponent>(first));
        assert(world.hasComponent<TestDeltaComponent>(second));
    }

    void testWorldEachVisitsEntitiesWithRequestedComponents()
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
        assert(world.getComponent<TestValueComponent>(destroyed) == nullptr);
    }

    void testWorldEachSupportsConstIteration()
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

    void testSystemSchedulerOrdersByPhasePriorityAndInsertion()
    {
        using namespace GameCore;

        Core::World world;
        Core::SystemScheduler scheduler;
        std::vector<int> updateOrder;

        scheduler.addSystem<RecordingSystem>(
            Core::SystemOrder{Core::SystemPhase::Render, 0},
            5,
            updateOrder);
        scheduler.addSystem<RecordingSystem>(
            Core::SystemOrder{Core::SystemPhase::Simulation, 10},
            4,
            updateOrder);
        scheduler.addSystem<RecordingSystem>(
            Core::SystemOrder{Core::SystemPhase::Input, 0},
            1,
            updateOrder);
        scheduler.addSystem<RecordingSystem>(
            Core::SystemOrder{Core::SystemPhase::Simulation, -10},
            2,
            updateOrder);
        scheduler.addSystem<RecordingSystem>(
            Core::SystemOrder{Core::SystemPhase::Simulation, -10},
            3,
            updateOrder);

        scheduler.update(world, Core::FrameContext{});

        assert((updateOrder == std::vector<int>{1, 2, 3, 4, 5}));
    }

    void testSystemSchedulerCanMutateWorld()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, TestValueComponent{5, 5});

        Core::SystemScheduler scheduler;
        scheduler.addSystem<ValueDecaySystem>();

        scheduler.update(world, Core::FrameContext{1.0F, 1});

        const auto* value = world.getComponent<TestValueComponent>(entity);
        assert(value != nullptr);
        assert(value->value == 4);

        scheduler.clear();
        assert(scheduler.systemCount() == 0);
    }

    void testMovementSystemAppliesAndClearsMoveIntent()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID entity = world.createEntity();
        world.addComponent(entity, Components::PositionComponent{2, 2});
        world.addComponent(entity, Components::MoveIntentComponent{4, -5});

        Systems::MovementSystem movement(0, 0, 5, 5);
        movement.update(world, Core::FrameContext{});

        const auto* position = world.getComponent<Components::PositionComponent>(entity);
        const auto* moveIntent = world.getComponent<Components::MoveIntentComponent>(entity);
        assert(position != nullptr);
        assert(moveIntent != nullptr);
        assert(position->x == 5);
        assert(position->y == 0);
        assert(moveIntent->dx == 0);
        assert(moveIntent->dy == 0);
    }

    void testTransformSystemComputesWorldTransforms()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID parent = world.createEntity();
        const Core::EntityID child = world.createEntity();

        world.addComponent(parent, Components::TransformComponent{10.0F, 4.0F});
        world.addComponent(child, Components::TransformComponent{3.0F, 2.0F, 0.5F, 2.0F, 3.0F, parent});

        Systems::TransformSystem transforms;
        transforms.update(world, Core::FrameContext{});

        const auto* parentWorld = world.getComponent<Components::WorldTransformComponent>(parent);
        const auto* childWorld = world.getComponent<Components::WorldTransformComponent>(child);

        assert(parentWorld != nullptr);
        assert(childWorld != nullptr);
        assert(parentWorld->x == 10.0F);
        assert(parentWorld->y == 4.0F);
        assert(childWorld->x == 13.0F);
        assert(childWorld->y == 6.0F);
        assert(childWorld->rotationRadians == 0.5F);
        assert(childWorld->scaleX == 2.0F);
        assert(childWorld->scaleY == 3.0F);
    }

    void testTransformSystemFallsBackToLocalTransformsForCycles()
    {
        using namespace GameCore;

        Core::World world;
        const Core::EntityID first = world.createEntity();
        const Core::EntityID second = world.createEntity();

        world.addComponent(first, Components::TransformComponent{10.0F, 4.0F, 0.0F, 1.0F, 1.0F, second});
        world.addComponent(second, Components::TransformComponent{3.0F, 2.0F, 0.5F, 2.0F, 3.0F, first});

        Systems::TransformSystem transforms;
        transforms.update(world, Core::FrameContext{});

        const auto* firstWorld = world.getComponent<Components::WorldTransformComponent>(first);
        const auto* secondWorld = world.getComponent<Components::WorldTransformComponent>(second);

        assert(firstWorld != nullptr);
        assert(secondWorld != nullptr);
        assert(firstWorld->x == 10.0F);
        assert(firstWorld->y == 4.0F);
        assert(secondWorld->x == 3.0F);
        assert(secondWorld->y == 2.0F);
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

    void testSceneRunsBeforeSystemsAndAfterSystemsHooksInOrder()
    {
        using namespace GameCore;

        std::vector<int> updateOrder;
        SequencedScene scene(updateOrder);
        scene.systems().addSystem<RecordingSystem>(2, updateOrder);

        scene.update(Core::FrameContext{0.016F, 4});

        assert((updateOrder == std::vector<int>{1, 2, 3}));
    }

    void testSceneBeforeSystemsIntentIsConsumedInSameFrame()
    {
        using namespace GameCore;

        BeforeSystemsMoveIntentScene scene;

        scene.update(Core::FrameContext{0.016F, 1});

        assert(scene.entityX() == 1);
    }

    void testSceneFlushesDeferredDestructionAfterSystems()
    {
        using namespace GameCore;

        LifecycleScene scene;
        const Core::EntityID entity = scene.world().createEntity();
        scene.world().addComponent(entity, TestValueComponent{1, 1});
        auto& destroyer = scene.systems().addSystem<DeferredDestroySystem>(entity);

        scene.update(Core::FrameContext{});

        assert(destroyer.wasAliveDuringUpdate());
        assert(!scene.world().isAlive(entity));
        assert(scene.world().deferredDestroyCount() == 0);
        assert(scene.world().getComponent<TestValueComponent>(entity) == nullptr);
    }

    void testSceneFlushesDeferredComponentMutationsAfterSystems()
    {
        DeferredMutationObservedScene scene;

        scene.update(GameCore::Core::FrameContext{});

        assert(scene.hadComponentAfterSystems());
        assert(scene.world().deferredComponentMutationCount() == 0);
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

    void testApplicationRunUsesOptionsAndMaxFrames()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<LifecycleScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.run(Core::ApplicationRunOptions{
            0.5F,
            2,
            false,
        });

        assert(!application.isRunning());
        assert(application.frameIndex() == 2);
        assert(application.totalSeconds() == 1.0F);
        assert(sceneView->initializeCount() == 1);
        assert(sceneView->updateCount() == 2);
    }

    void testApplicationRunAccumulatesFixedStepsFromClock()
    {
        using namespace GameCore;

        Core::Application application;
        SequenceClock clock({0.0, 0.25});
        auto scene = std::make_unique<ContextRecordingScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.run(Core::ApplicationRunOptions{
            0.1F,
            2,
            false,
            &clock,
        });

        assert(application.frameIndex() == 2);
        assert(application.fixedFrameIndex() == 2);
        assert(sceneView->contexts().size() == 2);
        assert(sceneView->contexts()[0].deltaSeconds == 0.1F);
        assert(sceneView->contexts()[0].frameIndex == 0);
        assert(sceneView->contexts()[0].fixedFrameIndex == 0);
        assert(sceneView->contexts()[1].frameIndex == 1);
        assert(sceneView->contexts()[1].fixedFrameIndex == 1);
    }

    void testApplicationRunLimitsFixedStepCatchUpPerLoop()
    {
        using namespace GameCore;

        Core::Application application;
        SequenceClock clock({0.0, 0.35, 0.35});
        auto scene = std::make_unique<LifecycleScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.run(Core::ApplicationRunOptions{
            0.1F,
            3,
            false,
            &clock,
            2,
        });

        assert(application.frameIndex() == 3);
        assert(sceneView->updateCount() == 3);
    }

    void testApplicationPauseStopsRunButAllowsManualStep()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<LifecycleScene>();
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.setPaused(true);
        application.runFrames(3, 0.5F);

        assert(application.isPaused());
        assert(application.frameIndex() == 0);
        assert(sceneView->updateCount() == 0);

        const bool stepped = application.stepFrame(0.5F);

        assert(stepped);
        assert(application.frameIndex() == 1);
        assert(application.fixedFrameIndex() == 1);
        assert(application.totalSeconds() == 0.5F);
        assert(sceneView->updateCount() == 1);
    }

    void testApplicationRunCanBeStoppedByScene()
    {
        using namespace GameCore;

        Core::Application application;
        auto scene = std::make_unique<StoppingScene>(application);
        auto* sceneView = scene.get();

        application.setScene(std::move(scene));
        application.run(Core::ApplicationRunOptions{
            1.0F,
            10,
            false,
        });

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

        assert(!input.hasAction("PrimaryAction"));
        assert(!input.wasPressed("PrimaryAction"));
        assert(!input.isHeld("PrimaryAction"));
        assert(!input.wasReleased("PrimaryAction"));

        input.setActionDown("PrimaryAction", true);

        assert(input.hasAction("PrimaryAction"));
        assert(input.wasPressed("PrimaryAction"));
        assert(input.isHeld("PrimaryAction"));
        assert(!input.wasReleased("PrimaryAction"));

        input.beginFrame();

        assert(!input.wasPressed("PrimaryAction"));
        assert(input.isHeld("PrimaryAction"));
        assert(!input.wasReleased("PrimaryAction"));

        input.setActionDown("PrimaryAction", false);

        assert(!input.wasPressed("PrimaryAction"));
        assert(!input.isHeld("PrimaryAction"));
        assert(input.wasReleased("PrimaryAction"));

        input.beginFrame();

        assert(!input.wasPressed("PrimaryAction"));
        assert(!input.isHeld("PrimaryAction"));
        assert(!input.wasReleased("PrimaryAction"));
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
        application.input().setActionDown("PrimaryAction", true);

        assert(application.input().wasPressed("PrimaryAction"));

        application.setScene(std::make_unique<LifecycleScene>());
        application.runFrames(1, 0.016F);

        assert(application.input().isHeld("PrimaryAction"));
        assert(!application.input().wasPressed("PrimaryAction"));
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

    void testResourceManagerTracksMetadataAndPublishesEvents()
    {
        using namespace GameCore;

        Core::ResourceManager resources;
        int loadedEvents = 0;
        bool sawReload = false;
        resources.events().subscribe<Core::ResourceLoadedEvent>(
            [&loadedEvents, &sawReload](const Core::ResourceLoadedEvent& event) {
                ++loadedEvents;
                sawReload = sawReload || event.reloaded;
            });

        int version = 0;
        resources.load<TextResource>(
            "text/versioned",
            [&version]() {
                ++version;
                return std::make_shared<TextResource>(TextResource{
                    "version " + std::to_string(version),
                });
            },
            "memory://versioned");

        assert(resources.reload<TextResource>("text/versioned"));

        const auto metadata = resources.metadata<TextResource>("text/versioned");
        assert(metadata.has_value());
        assert(metadata->id == "text/versioned");
        assert(metadata->sourcePath == "memory://versioned");
        assert(metadata->loadCount == 2);
        assert(metadata->reloadCount == 1);
        assert(metadata->lastReloadSucceeded);
        assert(metadata->lastError.empty());
        assert(loadedEvents == 2);
        assert(sawReload);
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

        const auto metadata = resources.metadata<TextResource>("config/reload-failure");
        assert(metadata.has_value());
        assert(!metadata->lastReloadSucceeded);
        assert(metadata->lastError == "reload failed");
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
            "# prefab demo\n"
            "[Player]\n"
            "position.x=0\n"
            "position.y=0\n"
            "\n"
            "[Enemy]\n"
            "position.x=1\n"
            "position.y=0\n";

        const Core::PrefabDocument document = Core::KeyValuePrefabLoader::loadFromText(text);

        assert(document.entities.size() == 2);
        assert(document.entities[0].name == "Player");
        assert(document.entities[0].position->x == 0);
        assert(document.entities[0].position->y == 0);

        assert(document.entities[1].name == "Enemy");
        assert(document.entities[1].position->x == 1);
        assert(document.entities[1].position->y == 0);
    }

    void testPrefabInstantiatorCreatesEntitiesWithComponents()
    {
        using namespace GameCore;

        Core::PrefabDocument document;
        document.entities.push_back(Core::EntityPrefab{
            "Player",
            Components::PositionComponent{0, 0},
        });
        document.entities.push_back(Core::EntityPrefab{
            "Marker",
            Components::PositionComponent{5, 6},
        });

        Core::World world;
        const auto entities = Core::PrefabInstantiator::instantiateAll(world, document);

        assert(entities.size() == 2);
        assert(world.livingCount() == 2);

        assert(world.getComponent<Components::PositionComponent>(entities[0])->x == 0);

        assert(world.getComponent<Components::PositionComponent>(entities[1])->x == 5);
        assert(world.getComponent<Components::PositionComponent>(entities[1])->y == 6);
    }

    void testPrefabRegistryInstantiatesCustomComponents()
    {
        using namespace GameCore;

        Core::PrefabComponentRegistry registry;
        registry.registerComponent("speed",
                                   [](Core::World& world,
                                      Core::EntityID entity,
                                      const Core::PrefabPropertyMap& properties) {
                                       world.addComponent(entity, SpeedComponent{
                                                                    std::stoi(properties.at("value")),
                                                                });
                                   });

        const auto document = Core::KeyValuePrefabLoader::loadFromText(
            "[Runner]\n"
            "speed.value=12\n",
            registry);

        assert(document.entities.size() == 1);
        assert(document.entities[0].runtimeComponents.size() == 1);
        assert(document.entities[0].runtimeComponents[0].type == "speed");
        assert(document.entities[0].runtimeComponents[0].properties.at("value") == "12");

        Core::World world;
        const auto entities = Core::PrefabInstantiator::instantiateAll(world, document);

        assert(entities.size() == 1);
        assert(world.getComponent<SpeedComponent>(entities[0])->value == 12);
    }

    void testKeyValuePrefabLoaderReportsInvalidInput()
    {
        using namespace GameCore;

        bool propertyBeforeSectionThrew = false;
        try
        {
            Core::KeyValuePrefabLoader::loadFromText("position.x=10\n");
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
            Core::KeyValuePrefabLoader::loadFromText("[Entity]\nposition.x=abc\n");
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

        const auto path = testAssetPath("scene.prefab");
        writeTextFile(path,
                      "[Player]\n"
                      "position.x=1\n");

        Core::ResourceManager resources;
        const auto oldHandle = Core::AssetLoaders::loadKeyValuePrefab(resources,
                                                                      "prefabs/scene",
                                                                      path.string());

        assert(oldHandle.isValid());
        assert(oldHandle->entities.size() == 1);
        assert(oldHandle->entities[0].position->x == 1);

        writeTextFile(path,
                      "[Player]\n"
                      "position.x=2\n"
                      "\n"
                      "[Enemy]\n"
                      "position.x=3\n");

        const bool reloaded = resources.reload<Core::PrefabDocument>("prefabs/scene");
        const auto newHandle = resources.require<Core::PrefabDocument>("prefabs/scene");

        assert(reloaded);
        assert(oldHandle->entities.size() == 1);
        assert(newHandle->entities.size() == 2);
        assert(newHandle->entities[0].position->x == 2);
        assert(newHandle->entities[1].name == "Enemy");

        std::filesystem::remove(path);
    }

    void testDiagnosticsSupportsLevelsAndCustomSink()
    {
        using namespace GameCore;

        Core::Diagnostics diagnostics;
        std::vector<std::string> messages;
        diagnostics.setSink([&messages](const Core::LogLevel level, const std::string_view message) {
            messages.emplace_back(std::to_string(static_cast<int>(level)) + ":" + std::string(message));
        });

        diagnostics.debug("debug");
        diagnostics.info("info");
        diagnostics.warning("warning");
        diagnostics.error("error");

        assert(messages.size() == 4);
        assert(messages[0] == "0:debug");
        assert(messages[1] == "1:info");
        assert(messages[2] == "2:warning");
        assert(messages[3] == "3:error");
    }

    void testDiagnosticsSupportsFilteringAndMultipleSinks()
    {
        using namespace GameCore;

        Core::Diagnostics diagnostics;
        diagnostics.clearSinks();
        diagnostics.setMinimumLevel(Core::LogLevel::Warning);

        int firstSinkCount = 0;
        int secondSinkCount = 0;
        const auto firstSink = diagnostics.addSink(
            [&firstSinkCount](Core::LogLevel, std::string_view) {
                ++firstSinkCount;
            });
        diagnostics.addSink(
            [&secondSinkCount](Core::LogLevel, std::string_view) {
                ++secondSinkCount;
            });

        diagnostics.info("filtered");
        diagnostics.warning("visible");
        diagnostics.removeSink(firstSink);
        diagnostics.error("visible");

        assert(diagnostics.minimumLevel() == Core::LogLevel::Warning);
        assert(diagnostics.sinkCount() == 1);
        assert(firstSinkCount == 1);
        assert(secondSinkCount == 2);
    }

}

int main()
{
    testEntityManagerRecyclesDestroyedEntities();
    testEntityManagerIgnoresInvalidAndDuplicateDestroy();
    testComponentStorageReplacesRemovesAndExposesMutableData();
    testRenderSystemIncludesAvailableComponentsOnly();
    testRenderSystemBuildsLayeredDrawCommands();
    testRenderSystemBuildsDrawCommandsFromRenderableComponents();
    testRenderSystemSkipsInvisibleRenderableComponents();
    testWorldRemovesComponentsWhenEntityIsDestroyed();
    testWorldRejectsComponentsForDeadEntities();
    testWorldRecycledEntityStartsWithoutOldComponents();
    testWorldRecycledEntityWrapsGenerationSafely();
    testWorldFlushesDeferredDestruction();
    testWorldDeduplicatesDeferredDestruction();
    testWorldFlushesDeferredComponentAddAndRemove();
    testWorldDeferredComponentAddSkipsDestroyedEntities();
    testWorldDeferredComponentMutationIsSafeDuringIteration();
    testWorldEachVisitsEntitiesWithRequestedComponents();
    testWorldEachSupportsConstIteration();
    testSystemSchedulerRunsSystemsInOrder();
    testSystemSchedulerOrdersByPhasePriorityAndInsertion();
    testSystemSchedulerCanMutateWorld();
    testMovementSystemAppliesAndClearsMoveIntent();
    testTransformSystemComputesWorldTransforms();
    testTransformSystemFallsBackToLocalTransformsForCycles();
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
    testSceneRunsBeforeSystemsAndAfterSystemsHooksInOrder();
    testSceneBeforeSystemsIntentIsConsumedInSameFrame();
    testSceneFlushesDeferredDestructionAfterSystems();
    testSceneFlushesDeferredComponentMutationsAfterSystems();
    testSceneShutdownClearsRuntimeOwnedSystemsAndEvents();
    testApplicationRunsActiveSceneForRequestedFrames();
    testApplicationRunWithoutSceneDoesNothing();
    testApplicationStopEndsFrameLoop();
    testApplicationRunUsesOptionsAndMaxFrames();
    testApplicationRunAccumulatesFixedStepsFromClock();
    testApplicationRunLimitsFixedStepCatchUpPerLoop();
    testApplicationPauseStopsRunButAllowsManualStep();
    testApplicationRunCanBeStoppedByScene();
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
    testResourceManagerTracksMetadataAndPublishesEvents();
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
    testPrefabRegistryInstantiatesCustomComponents();
    testKeyValuePrefabLoaderReportsInvalidInput();
    testPrefabAssetLoaderLoadsAndReloadsDocuments();
    testDiagnosticsSupportsLevelsAndCustomSink();
    testDiagnosticsSupportsFilteringAndMultipleSinks();
    return 0;
}
