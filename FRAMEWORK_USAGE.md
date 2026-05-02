# GameCoreCPP Framework Usage Guide

This guide explains how to use the current framework pieces together. The framework is a small engine foundation built around entities, components, systems, scenes, resources, prefabs, events, input, and an application loop.

## Build And Run

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/GameCoreDemo
```

The demo runs a playable combat encounter through the engine runtime and renders live frames through SDL2 when available. If SDL2 is not found, it falls back to terminal rendering.

SDL controls are `WASD`/arrow keys to move, `Space`/`Enter` to attack, and `Escape` or window close to exit.

Optional benchmark build:

```bash
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE -DGAMECORE_BUILD_BENCHMARKS=ON
cmake --build build-perf --target GameCoreRuntimeBenchmark
./build-perf/GameCoreRuntimeBenchmark
```

## Runtime Shape

```text
Application
├── InputManager
├── ResourceManager
└── active Scene
    ├── World
    │   ├── entities
    │   ├── component storages
    │   └── EventBus
    └── SystemScheduler
        └── ISystem implementations
```

`Application` owns app-level services. `SceneManager` creates scenes by name. Each `Scene` owns a `World` and a `SystemScheduler`.

## Application And Scenes

Create an application, register a scene, switch to it, and run frames:

```cpp
GameCore::Core::Application application;
GameCore::Core::SceneManager scenes;

scenes.registerScene("game", [] {
    return std::make_unique<MyGameScene>();
});

scenes.changeScene(application, "game");
application.runFrames(60, 1.0F / 60.0F);
```

Use `runFrames` for deterministic tests and tools. Use `run` for a runtime loop:

```cpp
application.run(GameCore::Core::ApplicationRunOptions{
    1.0F / 60.0F,
    0,
    true,
});
```

A scene derives from `GameCore::Core::Scene`:

```cpp
class MyGameScene final : public GameCore::Core::Scene
{
protected:
    void onInitialize() override
    {
        // Create entities, load resources, register systems.
    }

    void onBeforeSystems(const GameCore::Core::FrameContext& context) override
    {
        // Write input intent or scene commands before systems consume them.
    }

    void onUpdate(const GameCore::Core::FrameContext& context) override
    {
        // Compatibility hook called by the default onAfterSystems implementation.
    }

    void onAfterSystems(const GameCore::Core::FrameContext& context) override
    {
        // Scene-level work after scheduled systems and deferred destruction.
    }

    void onShutdown() override
    {
        // Optional cleanup.
    }
};
```

Scenes can access app-level services through `application()`:

```cpp
if (auto* app = application())
{
    app->stop();
    app->resources();
    app->input();
}
```

Frame order inside `Scene::update(...)` is:

```text
onBeforeSystems
SystemScheduler::update
World::flushDeferredDestruction
onAfterSystems
```

## Entities And Components

Entities are IDs. Components are plain data.

```cpp
auto player = world().createEntity();

world().addComponent(player, GameCore::Components::PositionComponent{0, 0});
world().addComponent(player, GameCore::Components::MoveIntentComponent{});
world().addComponent(player, GameCore::Components::NameComponent{"Player"});
world().addComponent(player, GameCore::Components::RenderableComponent{'P'});
world().addComponent(player, GameCore::Components::ProgressComponent{120, 120});
```

When an entity is destroyed through `World`, its components are removed too:

```cpp
world().destroyEntity(player);
```

Systems that are iterating over component data can queue destruction for the scene boundary:

```cpp
world().deferDestroyEntity(player);
```

The active `Scene` flushes deferred destruction after scheduled systems finish for the frame.

Current built-in generic components:

- `PositionComponent`
- `MoveIntentComponent`
- `NameComponent`
- `RenderableComponent`
- `ProgressComponent`
- `TransformComponent`
- `WorldTransformComponent`

Game-specific components, such as combat health or attack data, should live in the game/demo layer.

## Systems

Frame systems implement `ISystem` and are added to a scene scheduler:

```cpp
class MovementSystem final : public GameCore::Core::ISystem
{
public:
    void update(GameCore::Core::World& world,
                const GameCore::Core::FrameContext& context) override
    {
        world.each<GameCore::Components::PositionComponent>(
            [](GameCore::Core::EntityID, GameCore::Components::PositionComponent& position) {
                position.x += 1;
            });
    }
};

systems().addSystem<MovementSystem>();
```

Systems run in registration order by default. They can also be ordered by phase and priority:

```cpp
systems().addSystem<MovementSystem>(
    GameCore::Core::SystemOrder{GameCore::Core::SystemPhase::Simulation, 10});
```

Built-in systems currently include:

- `MovementSystem`
- `TransformSystem`
- `RenderSystem`

## Events

Use `EventBus` for decoupled communication.

```cpp
struct DamageEvent
{
    GameCore::Core::EntityID target;
    int amount;
};

auto listener = world().events().subscribe<DamageEvent>([](const DamageEvent& event) {
    // React to damage.
});

world().events().publish(DamageEvent{target, 10});
world().events().unsubscribe<DamageEvent>(listener);
```

## Input

`InputManager` stores action state. SDL keyboard/window events can feed it through `SdlInputBackend` when SDL2 is available. Future controller, terminal, scripted, or test input backends should translate raw input into the same named action contract.

```cpp
application.input().setActionDown("PrimaryAction", true);

if (application.input().wasPressed("PrimaryAction"))
{
    // Trigger the primary action once.
}

if (application.input().isHeld("MoveUp"))
{
    // Continue moving.
}

if (application.input().wasReleased("PrimaryAction"))
{
    // Handle release.
}
```

`Application::runFrames` advances input frame state after each scene update.

## Resources And Assets

`ResourceManager` caches typed resources by string ID.

```cpp
auto text = application.resources().loadValue<MyData>("data/player", [] {
    return MyData{};
});
```

Built-in asset helpers:

```cpp
auto text = GameCore::Core::AssetLoaders::loadText(
    application.resources(),
    "dialogue/intro",
    "assets/dialogue/intro.txt");

auto binary = GameCore::Core::AssetLoaders::loadBinary(
    application.resources(),
    "blob/data",
    "assets/data.bin");
```

Reloading replaces the cached resource for future lookups. Existing handles remain valid because they hold `std::shared_ptr`.

Resource metadata and reload events are available:

```cpp
auto metadata = application.resources().metadata<MyData>("data/player");
application.resources().events().subscribe<GameCore::Core::ResourceLoadedEvent>(
    [](const GameCore::Core::ResourceLoadedEvent& event) {
        // Inspect event.metadata and event.reloaded.
    });
```

## Diagnostics

`Diagnostics` supports level filtering and configurable sinks:

```cpp
application.diagnostics().setMinimumLevel(GameCore::Core::LogLevel::Warning);
application.diagnostics().warning("Missing optional asset");
```

## Prefabs

`EntityPrefab` is the format-neutral in-memory model. `KeyValuePrefabLoader` is only the first concrete loader. JSON or YAML can later produce the same `PrefabDocument`.

Example prefab file:

```text
[Player]
position.x=0
position.y=0

[Enemy]
position.x=1
position.y=0
```

Load and instantiate:

```cpp
auto prefab = GameCore::Core::AssetLoaders::loadKeyValuePrefab(
    application.resources(),
    "prefabs/combat",
    "assets/combat.prefab");

auto entities = GameCore::Core::PrefabInstantiator::instantiateAll(world(), *prefab);
```

Current prefab component support:

- `PositionComponent`
- Custom registered components through `PrefabComponentRegistry`

Use `PrefabComponentRegistry` for game-owned data:

```cpp
GameCore::Core::PrefabComponentRegistry registry;
registry.registerComponent("health", [](GameCore::Core::World& world,
                                        GameCore::Core::EntityID entity,
                                        const GameCore::Core::PrefabPropertyMap& properties) {
    // Add game-specific components here.
});
```

## State Machines

Use `StateMachine<StateID>` for game states, AI, UI screens, turn phases, or runtime modes.

```cpp
enum class EnemyState
{
    Idle,
    Attack,
};

GameCore::Core::StateMachine<EnemyState> machine;

machine.addState(EnemyState::Idle, {
    [](GameCore::Core::World&) {},
    [](GameCore::Core::World&) {},
    [](GameCore::Core::World&, const GameCore::Core::FrameContext&) {},
});

machine.addState(EnemyState::Attack, {});
machine.allowTransition(EnemyState::Idle, EnemyState::Attack);
machine.setInitialState(EnemyState::Idle, world());
machine.changeState(EnemyState::Attack, world());
machine.update(world(), context);
```

## Current Demo Flow

`demo/main.cpp`:

1. Creates `Application`.
2. Registers `CombatDemoScene` in `SceneManager`.
3. Loads `demo/assets/combat_demo.prefab`.
4. Instantiates player and enemy into the scene `World`.
5. Adds movement, demo combat intent, and render systems.
6. Publishes combat log events.
7. Renders generic render/progress component state every frame through `RenderSystem`.
8. Stops the application when combat ends.

## Recommended Extension Order

1. Add additional input backends, especially controller, terminal, scripted, and test backends.
2. Add the runtime time/fixed-step layer with pause/manual stepping.
3. Add deferred component add/remove commands.
4. Add scene description loading.
5. Add serialization for saving/loading world state.
6. Add camera/sprite rendering on top of draw commands.
7. Add rendering/audio/physics backends when the data/runtime contracts are stable.
