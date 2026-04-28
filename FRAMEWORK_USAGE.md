# GameCoreCPP Framework Usage Guide

This guide explains how to use the current framework pieces together. The framework is a small engine foundation built around entities, components, systems, scenes, resources, prefabs, events, input, and an application loop.

## Build And Run

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/GameCoreDemo
```

The demo runs a terminal combat encounter through the engine runtime.

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

A scene derives from `GameCore::Core::Scene`:

```cpp
class MyGameScene final : public GameCore::Core::Scene
{
protected:
    void onInitialize() override
    {
        // Create entities, load resources, register systems.
    }

    void onUpdate(const GameCore::Core::FrameContext& context) override
    {
        // Scene-level work after scheduled systems run.
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

## Entities And Components

Entities are IDs. Components are plain data.

```cpp
auto player = world().createEntity();

world().addComponent(player, GameCore::Components::HealthComponent{120, 120});
world().addComponent(player, GameCore::Components::AttackComponent{25});
world().addComponent(player, GameCore::Components::PositionComponent{0, 0});
```

When an entity is destroyed through `World`, its components are removed too:

```cpp
world().destroyEntity(player);
```

## Systems

Frame systems implement `ISystem` and are added to a scene scheduler:

```cpp
class MovementSystem final : public GameCore::Core::ISystem
{
public:
    void update(GameCore::Core::World& world,
                const GameCore::Core::FrameContext& context) override
    {
        auto& positions = world.storage<GameCore::Components::PositionComponent>();
        for (auto& entry : positions.all())
        {
            entry.second.x += 1;
        }
    }
};

systems().addSystem<MovementSystem>();
```

Systems run in registration order every scene update.

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

`InputManager` stores action state. It does not read keyboard/controller devices yet. A future backend should translate raw input into named actions.

```cpp
application.input().setActionDown("Attack", true);

if (application.input().wasPressed("Attack"))
{
    // Trigger attack once.
}

if (application.input().isHeld("MoveUp"))
{
    // Continue moving.
}

if (application.input().wasReleased("Attack"))
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

## Prefabs

`EntityPrefab` is the format-neutral in-memory model. `KeyValuePrefabLoader` is only the first concrete loader. JSON or YAML can later produce the same `PrefabDocument`.

Example prefab file:

```text
[Player]
health.current=120
health.max=120
attack.damage=25
position.x=0
position.y=0

[Enemy]
health.current=90
health.max=90
attack.damage=15
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

- `HealthComponent`
- `AttackComponent`
- `PositionComponent`

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
5. Adds `CombatRoundSystem`.
6. Publishes combat log events.
7. Renders combat state.
8. Stops the application when combat ends.

## Recommended Extension Order

1. Add real input backends that feed `InputManager`.
2. Add scene description loading.
3. Add serialization for saving/loading world state.
4. Add movement and gameplay systems.
5. Add rendering/audio/physics backends when the data/runtime contracts are stable.
