#include "GameCore/Components/MoveIntentComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Components/ProgressComponent.h"
#include "GameCore/Components/RenderableComponent.h"
#include "GameCore/Core/EventBus.h"
#include "GameCore/Core/System.h"
#include "GameCore/Core/World.h"
#include "GameCore/Systems/MovementSystem.h"
#include "GameCore/Systems/RenderSystem.h"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct BenchmarkResult
    {
        std::string name;
        double milliseconds{0.0};
        std::uint64_t operations{0};
        std::int64_t checksum{0};
    };

    struct BenchmarkEvent
    {
        int amount{0};
    };

    template <typename Function>
    BenchmarkResult measure(std::string name, const std::uint64_t operations, Function function)
    {
        const auto start = Clock::now();
        const std::int64_t checksum = function();
        const auto end = Clock::now();
        const auto elapsed = std::chrono::duration<double, std::milli>(end - start);
        return BenchmarkResult{std::move(name), elapsed.count(), operations, checksum};
    }

    void printResult(const BenchmarkResult& result)
    {
        const double operationsPerSecond =
            result.milliseconds <= 0.0
                ? 0.0
                : (static_cast<double>(result.operations) / result.milliseconds) * 1000.0;

        std::cout << std::left << std::setw(34) << result.name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(3)
                  << result.milliseconds << " ms"
                  << std::setw(16) << std::setprecision(0) << operationsPerSecond << " ops/s"
                  << "  checksum=" << result.checksum << '\n';
    }

    GameCore::Core::World makeWorld(const int entityCount)
    {
        GameCore::Core::World world;
        for (int i = 0; i < entityCount; ++i)
        {
            const auto entity = world.createEntity();
            world.addComponent(entity, GameCore::Components::PositionComponent{i % 128, i / 128});
            world.addComponent(entity, GameCore::Components::MoveIntentComponent{1, 0});
            world.addComponent(entity, GameCore::Components::ProgressComponent{75, 100});
            world.addComponent(entity, GameCore::Components::RenderableComponent{
                                           static_cast<char>('A' + (i % 26)),
                                           i % 4,
                                           true,
                                       });
        }
        return world;
    }
}

int main()
{
    constexpr int EntityCount = 50000;
    constexpr int Iterations = 200;
    constexpr int EventPublishes = 1000000;

    std::cout << "GameCoreCPP runtime benchmark\n"
              << "entities=" << EntityCount
              << " iterations=" << Iterations << "\n\n";

    printResult(measure("create/add/destroy entities",
                        static_cast<std::uint64_t>(EntityCount) * 4,
                        []() {
                            GameCore::Core::World world;
                            std::vector<GameCore::Core::EntityID> entities;
                            entities.reserve(EntityCount);

                            std::int64_t checksum = 0;
                            for (int i = 0; i < EntityCount; ++i)
                            {
                                const auto entity = world.createEntity();
                                entities.push_back(entity);
                                world.addComponent(entity, GameCore::Components::PositionComponent{i, i});
                                world.addComponent(entity, GameCore::Components::MoveIntentComponent{1, 0});
                                checksum += static_cast<std::int64_t>(entity);
                            }

                            for (const auto entity : entities)
                            {
                                world.destroyEntity(entity);
                            }

                            return checksum + static_cast<std::int64_t>(world.livingCount());
                        }));

    {
        auto world = makeWorld(EntityCount);
        printResult(measure("world.each position+intent",
                            static_cast<std::uint64_t>(EntityCount) * Iterations,
                            [&world]() {
                                std::int64_t checksum = 0;
                                for (int iteration = 0; iteration < Iterations; ++iteration)
                                {
                                    world.each<GameCore::Components::PositionComponent,
                                               GameCore::Components::MoveIntentComponent>(
                                        [&checksum](GameCore::Core::EntityID,
                                                    GameCore::Components::PositionComponent& position,
                                                    GameCore::Components::MoveIntentComponent& intent) {
                                            position.x += intent.dx;
                                            checksum += position.x;
                                        });
                                }
                                return checksum;
                            }));
    }

    {
        auto world = makeWorld(EntityCount);
        GameCore::Systems::MovementSystem movement(0, 0, 1000000, 1000000);

        printResult(measure("MovementSystem update",
                            static_cast<std::uint64_t>(EntityCount) * Iterations,
                            [&world, &movement]() {
                                std::int64_t checksum = 0;
                                for (int iteration = 0; iteration < Iterations; ++iteration)
                                {
                                    auto& intents =
                                        world.storage<GameCore::Components::MoveIntentComponent>();
                                    for (auto& entry : intents.all())
                                    {
                                        entry.second.dx = 1;
                                    }

                                    movement.update(world, GameCore::Core::FrameContext{0.016F,
                                                                                        static_cast<std::uint64_t>(iteration)});
                                }

                                world.each<GameCore::Components::PositionComponent>(
                                    [&checksum](GameCore::Core::EntityID,
                                                const GameCore::Components::PositionComponent& position) {
                                        checksum += position.x;
                                    });
                                return checksum;
                            }));
    }

    {
        GameCore::Core::EventBus events;
        std::int64_t total = 0;
        events.subscribe<BenchmarkEvent>([&total](const BenchmarkEvent& event) {
            total += event.amount;
        });

        printResult(measure("EventBus publish one listener",
                            EventPublishes,
                            [&events, &total]() {
                                for (int i = 0; i < EventPublishes; ++i)
                                {
                                    events.publish(BenchmarkEvent{1});
                                }
                                return total;
                            }));
    }

    {
        auto world = makeWorld(EntityCount);

        printResult(measure("buildDrawFrame commands",
                            static_cast<std::uint64_t>(EntityCount) * Iterations,
                            [&world]() {
                                std::int64_t checksum = 0;
                                for (int iteration = 0; iteration < Iterations; ++iteration)
                                {
                                    const auto frame = GameCore::Systems::buildDrawFrame(
                                        world,
                                        GameCore::Core::FrameContext{0.016F,
                                                                    static_cast<std::uint64_t>(iteration)});
                                    checksum += static_cast<std::int64_t>(frame.commands.size());
                                }
                                return checksum;
                            }));
    }

    return 0;
}
