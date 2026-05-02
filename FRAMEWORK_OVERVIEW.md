# GameCoreCPP Framework Overview

This document explains how the framework currently works and how to use its main systems together.

For the detailed record of the latest architecture session, see `CODING_SESSION_CHANGES.md`.

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
- diagnostics
- backend-neutral rendering

## 1. The Big Idea

The framework is built around an ECS-style architecture:

```text
Entity    = an ID
Component = data attached to an entity
System    = behavior that operates on components
```

For example, a player is not a `Player` class. A player is an entity ID with components:

```cpp
world.addComponent(player, PositionComponent{0, 0});
world.addComponent(player, MoveIntentComponent{});
world.addComponent(player, NameComponent{"Player"});
world.addComponent(player, RenderableComponent{'P'});
world.addComponent(player, ProgressComponent{120, 120});
```

Systems read and modify those components.

## 2. Runtime Structure

The current runtime looks like this:

```text
Application
├── InputManager
├── ResourceManager
├── Diagnostics
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
Systems can use explicit phases and priorities when registration order is not enough.
The scheduler only re-sorts when systems are registered.

`RenderSystem` converts component state into render frames and sends them to an `IRenderBackend`. The current backends are SDL2 for a windowed arena and terminal rendering as a fallback; future backends can target sprites, 3D, or other platforms without changing gameplay systems.

## 3. Application

`Application` is the top-level runtime object.

It owns:

- the active scene
- shared resources
- input state
- frame index
- accumulated runtime

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

Every system receives a `FrameContext` containing delta time, frame index, total elapsed time, and fixed-frame index. The fixed-frame model is not complete yet, but the data contract is in place for the next timing pass.

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

    void onBeforeSystems(const GameCore::Core::FrameContext& context) override
    {
        // Write input intent before scheduled systems run.
    }

    void onUpdate(const GameCore::Core::FrameContext& context) override
    {
        // Compatibility hook called by default onAfterSystems.
    }

    void onAfterSystems(const GameCore::Core::FrameContext& context) override
    {
        // Scene-level logic after systems and deferred destruction.
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
world.addComponent(entity, PositionComponent{5, 2});
world.addComponent(entity, ProgressComponent{50, 100});
```

Read components:

```cpp
auto* position = world.getComponent<PositionComponent>(entity);
```

Destroy an entity:

```cpp
world.destroyEntity(entity);
```

When an entity is destroyed, its components are removed from all component storages.

Systems can also queue destruction safely:

```cpp
world.deferDestroyEntity(entity);
```

`Scene` flushes deferred destruction after scheduled systems have run for the frame.

## 7. Components

Current built-in components:

- `PositionComponent`
- `MoveIntentComponent`
- `ProgressComponent`
- `NameComponent`
- `RenderableComponent`
- `TransformComponent`
- `WorldTransformComponent`

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

Built-in systems currently include `MovementSystem`, `TransformSystem`, and `RenderSystem`.
Systems run in registration order by default, or by explicit phase and priority when supplied.

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

SDL keyboard/window events can feed it through `SdlInputBackend` when SDL2 is available. Future controller, terminal, scripted, and test input backends should translate raw input into the same action names.

Set input:

```cpp
application.input().setActionDown("PrimaryAction", true);
```

Query input:

```cpp
if (application.input().wasPressed("PrimaryAction"))
{
    // Primary action started this frame.
}

if (application.input().isHeld("MoveUp"))
{
    // Move while held.
}

if (application.input().wasReleased("PrimaryAction"))
{
    // Primary action released this frame.
}
```

The application advances input frame state after each frame.

Current SDL mappings include:

- `WASD` and arrow keys to movement actions.
- `Space` and `Enter` to `PrimaryAction`.
- `Escape` and window close to `Quit`.

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

- position
- custom registered component prefixes

Example prefab file:

```text
[Player]
position.x=0
position.y=0
```

Sample gameplay can register custom prefixes such as `health.*` and `attack.*` through `PrefabComponentRegistry`.

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
adds movement, demo combat intent, and render systems
uses generic render/progress components for display
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

## 17. What Is Left

The next engine work should stay focused on runtime contracts rather than adding more one-off demo behavior:

1. Add a real time/fixed-step layer with pause and manual stepping.
2. Add deferred component add/remove commands.
3. Add camera and sprite/render components on top of `DrawFrame`.
4. Add scene description assets that instantiate prefabs and configure systems.
5. Strengthen prefab validation and add JSON/YAML loaders once the data model settles.
6. Continue splitting tests by subsystem as coverage grows.

## 18. Current Limitations

The framework does not yet include:

- controller or terminal input backend
- sprite, texture, text, camera, or material rendering
- audio
- physics
- save/load serialization
- JSON/YAML parsing
- editor tooling

The current goal is to keep the engine foundation clean before adding those heavier systems. SDL2 already provides the first native input/window/render path for the demo, but that path is still intentionally simple.
