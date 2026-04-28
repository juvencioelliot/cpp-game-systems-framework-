# GameCoreCPP Framework Overview

This document explains how the framework currently works and how to use its main systems together.

GameCoreCPP is a small C++ game framework foundation. It is not a full engine yet, but it already has the core runtime pieces needed to build toward one:

- application loop
- scenes
- entities and components
- systems
- events
- input actions
- resources and assets
- prefabs
- state machines

## 1. The Big Idea

The framework is built around an ECS-style architecture:

```text
Entity    = an ID
Component = data attached to an entity
System    = behavior that operates on components
```

For example, a player is not a `Player` class. A player is an entity ID with components:

```cpp
world.addComponent(player, HealthComponent{120, 120});
world.addComponent(player, AttackComponent{25});
world.addComponent(player, PositionComponent{0, 0});
```

Systems read and modify those components.

## 2. Runtime Structure

The current runtime looks like this:

```text
Application
├── InputManager
├── ResourceManager
└── Active Scene
    ├── World
    │   ├── EntityManager
    │   ├── ComponentStorage<T>
    │   └── EventBus
    └── SystemScheduler
        └── ISystem instances
```

The `Application` owns app-level services and updates the active scene.

The `Scene` owns gameplay state for a level, menu, battle, or other runtime unit.

The `World` owns entities, components, and events.

The `SystemScheduler` runs systems in order every frame.

## 3. Application

`Application` is the top-level runtime object.

It owns:

- the active scene
- shared resources
- input state
- frame index

Typical usage:

```cpp
GameCore::Core::Application application;
GameCore::Core::SceneManager scenes;

scenes.registerScene("combat_demo", [] {
    return std::make_unique<CombatDemoScene>();
});

scenes.changeScene(application, "combat_demo");
application.runFrames(16, 1.0F);
```

`runFrames` updates the active scene repeatedly.

## 4. Scenes

A scene is a runtime container. It owns a `World` and a `SystemScheduler`.

Create a scene by deriving from `Scene`:

```cpp
class MyScene final : public GameCore::Core::Scene
{
protected:
    void onInitialize() override
    {
        // Create entities, load assets, register systems.
    }

    void onUpdate(const GameCore::Core::FrameContext& context) override
    {
        // Scene-level logic after systems update.
    }

    void onShutdown() override
    {
        // Cleanup.
    }
};
```

Scenes can access the application through:

```cpp
application()
```

That gives access to:

```cpp
application()->stop();
application()->resources();
application()->input();
```

## 5. SceneManager

`SceneManager` registers named scene factories.

```cpp
scenes.registerScene("menu", [] {
    return std::make_unique<MenuScene>();
});

scenes.changeScene(application, "menu");
```

This keeps scene creation separate from the application loop.

## 6. World

`World` owns entity and component state.

Create an entity:

```cpp
auto entity = world.createEntity();
```

Add components:

```cpp
world.addComponent(entity, HealthComponent{100, 100});
world.addComponent(entity, PositionComponent{5, 2});
```

Read components:

```cpp
auto* health = world.getComponent<HealthComponent>(entity);
```

Destroy an entity:

```cpp
world.destroyEntity(entity);
```

When an entity is destroyed, its components are removed from all component storages.

## 7. Components

Current built-in components:

- `HealthComponent`
- `AttackComponent`
- `PositionComponent`

Components are plain data. They should not own game behavior.

## 8. Systems

Systems contain behavior.

Frame-based systems implement `ISystem`:

```cpp
class MySystem final : public GameCore::Core::ISystem
{
public:
    void update(GameCore::Core::World& world,
                const GameCore::Core::FrameContext& context) override
    {
        // Process component data.
    }
};
```

Register a system inside a scene:

```cpp
systems().addSystem<MySystem>();
```

Systems run in registration order.

## 9. Events

`EventBus` is used for decoupled communication.

Define an event:

```cpp
struct DamageEvent
{
    GameCore::Core::EntityID target;
    int amount;
};
```

Subscribe:

```cpp
auto listener = world.events().subscribe<DamageEvent>([](const DamageEvent& event) {
    // Handle event.
});
```

Publish:

```cpp
world.events().publish(DamageEvent{target, 10});
```

Unsubscribe:

```cpp
world.events().unsubscribe<DamageEvent>(listener);
```

## 10. Input

`InputManager` tracks named actions.

It does not read real keyboard/controller input yet. A future platform backend should translate raw input into action names.

Set input:

```cpp
application.input().setActionDown("Attack", true);
```

Query input:

```cpp
if (application.input().wasPressed("Attack"))
{
    // Attack started this frame.
}

if (application.input().isHeld("MoveUp"))
{
    // Move while held.
}

if (application.input().wasReleased("Attack"))
{
    // Attack released this frame.
}
```

The application advances input frame state after each frame.

## 11. Resources

`ResourceManager` stores typed resources by ID.

It supports:

- load
- get
- require
- reload
- unload
- clear

Example:

```cpp
auto handle = application.resources().loadValue<MyData>("data/player", [] {
    return MyData{};
});
```

Resources are held through `std::shared_ptr`.

Reloading a resource updates the cached version for future lookups. Existing handles remain valid.

## 12. File And Asset Loading

`FileSystem` handles raw file IO:

```cpp
FileSystem::exists(path);
FileSystem::readTextFile(path);
FileSystem::readBinaryFile(path);
```

`AssetLoaders` converts files into resources:

```cpp
AssetLoaders::loadText(resources, "dialogue/intro", "assets/intro.txt");
AssetLoaders::loadBinary(resources, "data/blob", "assets/blob.bin");
AssetLoaders::loadKeyValuePrefab(resources, "prefabs/combat", "assets/combat.prefab");
```

## 13. Prefabs

Prefabs are data-driven entity definitions.

The in-memory model is:

```cpp
EntityPrefab
PrefabDocument
```

Current supported prefab components:

- health
- attack
- position

Example prefab file:

```text
[Player]
health.current=120
health.max=120
attack.damage=25
position.x=0
position.y=0
```

Load and instantiate:

```cpp
auto prefab = AssetLoaders::loadKeyValuePrefab(
    application.resources(),
    "prefabs/combat",
    "assets/combat.prefab");

auto entities = PrefabInstantiator::instantiateAll(world(), *prefab);
```

The important design point: the engine depends on `PrefabDocument`, not the file format. JSON or YAML loaders can be added later by producing the same data model.

## 14. State Machines

`StateMachine<StateID>` supports state-driven behavior.

Use it for:

- AI states
- UI screens
- turn phases
- animation flow
- runtime modes

Example:

```cpp
enum class EnemyState
{
    Idle,
    Attack,
};

GameCore::Core::StateMachine<EnemyState> stateMachine;

stateMachine.addState(EnemyState::Idle, {});
stateMachine.addState(EnemyState::Attack, {});
stateMachine.allowTransition(EnemyState::Idle, EnemyState::Attack);
stateMachine.setInitialState(EnemyState::Idle, world);
stateMachine.changeState(EnemyState::Attack, world);
```

## 15. Current Demo

The demo is in:

```text
demo/main.cpp
demo/assets/combat_demo.prefab
```

The flow is:

```text
main
creates Application
registers CombatDemoScene
loads combat_demo.prefab
instantiates Player and Enemy
adds CombatRoundSystem
runs combat one round per frame
publishes log events
renders state
stops when combat ends
```

Run it:

```bash
./build/GameCoreDemo
```

## 16. How To Add A New Game Feature

For a new gameplay feature, usually follow this order:

1. Add a component for the data.
2. Add a system for the behavior.
3. Register the system in a scene.
4. Add prefab fields if the component should be data-driven.
5. Use events if other systems need to react.
6. Use resources/assets if the feature needs external data.

Example:

```text
VelocityComponent
MovementSystem
position + velocity prefab fields
MovementStartedEvent
```

## 17. Current Limitations

The framework does not yet include:

- real keyboard/controller backend
- windowing
- graphics rendering
- audio
- physics
- save/load serialization
- JSON/YAML parsing
- editor tooling

The current goal is to keep the engine foundation clean before adding those heavier systems.
