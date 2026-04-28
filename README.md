# GameCoreCPP

GameCoreCPP is a modular C++ game systems framework. It is not trying to clone Unity or Unreal feature-for-feature yet; it is building the kind of clean core those engines depend on: entity lifetime, component ownership, system-driven behavior, and a small but testable runtime layer.

The current demo shows a player and an enemy fighting in the terminal through an engine-hosted scene, scheduled system, and event-driven log output.

## Goals

- Demonstrate modern, readable C++ architecture.
- Keep entity, component, and system responsibilities separate.
- Use CMake so the project builds from the command line.
- Build a strong foundation that can evolve toward a larger engine-style runtime.
- Keep entity lifetime and component ownership centralized through `World`.

## Quick Tutorial: Build and Run

This project uses CMake and requires a C++17-capable compiler.

### 1. Clone and enter the project

```bash
git clone <repository-url>
cd cpp-game-systems-framework-
```

If you already have the repository locally, just open a terminal from the repository root.

### 2. Configure the build

Generate the build files in a local `build` directory:

```bash
cmake -S . -B build
```

### 3. Compile the project

Build the framework library, demo executable, and test executable:

```bash
cmake --build build
```

### 4. Run the demo

Start the terminal combat demo:

```bash
./build/GameCoreDemo
```

The demo creates a player and enemy, assigns health, attack, and position components, then runs a simple combat loop.

### 5. Run the tests

After building, run the test suite with CTest:

```bash
ctest --test-dir build
```

## Current Architecture

```text
GameCoreCPP/
├── CMakeLists.txt
├── include/
│   └── GameCore/
│       ├── Core/
│       │   ├── ApplicationContext.h
│       │   ├── Application.h
│       │   ├── AssetLoaders.h
│       │   ├── ComponentStorage.h
│       │   ├── Entity.h
│       │   ├── EntityManager.h
│       │   ├── EntityPrefab.h
│       │   ├── EventBus.h
│       │   ├── FileSystem.h
│       │   ├── InputManager.h
│       │   ├── KeyValuePrefabLoader.h
│       │   ├── ResourceManager.h
│       │   ├── Scene.h
│       │   ├── SceneManager.h
│       │   ├── StateMachine.h
│       │   ├── System.h
│       │   ├── SystemScheduler.h
│       │   └── World.h
│       ├── Components/
│       │   ├── AttackComponent.h
│       │   ├── HealthComponent.h
│       │   └── PositionComponent.h
│       └── Systems/
│           ├── CombatSystem.h
│           └── RenderSystem.h
├── src/
│   ├── Core/
│   │   ├── Application.cpp
│   │   ├── AssetLoaders.cpp
│   │   ├── EntityManager.cpp
│   │   ├── EntityPrefab.cpp
│   │   ├── FileSystem.cpp
│   │   ├── KeyValuePrefabLoader.cpp
│   │   ├── Scene.cpp
│   │   ├── SceneManager.cpp
│   │   └── SystemScheduler.cpp
│   └── Systems/
│       ├── CombatSystem.cpp
│       └── RenderSystem.cpp
├── demo/
│   └── main.cpp
└── tests/
    └── basic_tests.cpp
```

### Core

- `Application` owns the active scene and drives deterministic frame updates.
- `ApplicationContext` is the limited interface scenes use to request app-level services.
- `AssetLoaders` provides file-backed resource loader helpers for built-in resource types.
- `EntityPrefab` defines a format-neutral prefab data model and instantiation helpers.
- `Entity.h` defines `EntityID`, the lightweight identifier used to represent game objects.
- `EntityManager` creates, destroys, tracks, and recycles entity IDs.
- `ComponentStorage<T>` is a small generic storage class backed by `std::unordered_map<EntityID, T>`.
- `EventBus` provides typed publish/subscribe messaging for decoupled runtime communication.
- `FileSystem` provides basic text and binary file reads.
- `InputManager` tracks backend-agnostic action states across frames.
- `KeyValuePrefabLoader` parses simple prefab files into the prefab data model.
- `ResourceManager` loads, caches, retrieves, reloads, and unloads typed resources by string ID.
- `Scene` groups a `World`, a `SystemScheduler`, and lifecycle hooks into a runtime unit.
- `SceneManager` registers named scene factories and switches the active application scene.
- `StateMachine<StateID>` provides reusable state lifecycle and transition handling for gameplay, AI, UI, and runtime modes.
- `FrameContext` carries per-frame timing and frame index data into runtime systems.
- `ISystem` defines the update contract for systems that run every frame.
- `SystemScheduler` owns runtime systems and updates them in explicit registration order.
- `World` is the main runtime facade. It owns entity lifetime, creates component storages on demand, removes an entity's components when that entity is destroyed, and exposes the event bus.

### Components

Components are simple data structs:

- `HealthComponent` stores current and maximum health.
- `AttackComponent` stores attack damage.
- `PositionComponent` stores a simple grid position.

### Systems

Systems contain behavior:

- `CombatSystem` reads attack components and updates health components.
- `RenderSystem` reads component state and prints a terminal view.
- Runtime systems can implement `ISystem` and be driven by `SystemScheduler`.

### State Machines

The framework provides a generic `StateMachine<StateID>`. It supports registered states, enter/update/exit callbacks, current-state tracking, optional transition rules, and clearing. The engine provides the mechanism; game projects define their own state IDs and behavior.

### Scenes

`Scene` is the runtime unit that groups a `World` and `SystemScheduler`. Scenes can be initialized, updated, and shut down, with overridable lifecycle hooks for game-specific setup and teardown.

### Application

`Application` owns the active scene, shared resources, input state, and frame updates with `FrameContext`. It can replace scenes, stop the frame loop, and track the current frame index.

### Input

`InputManager` tracks named actions such as `Attack`, `Confirm`, or `MoveUp`. It is backend-agnostic: platform code can set action states, and scenes/systems can query `wasPressed`, `isHeld`, and `wasReleased`.

### Resources

`ResourceManager` is a type-safe cache for engine and game data. It stores resources by type and string ID, uses caller-provided loader functions, and returns `ResourceHandle<T>` values backed by `std::shared_ptr`.

Current behavior:

- Loading the same type and ID twice returns the cached resource.
- Different resource types can use the same ID without colliding.
- Resources can be queried, required, unloaded, cleared, and reloaded.
- Reloading replaces the cached resource for future lookups; existing handles remain valid references to the older resource.
- `Application` owns a shared resource manager so resources can survive scene changes.

Built-in file-backed resource types:

- `TextResource`: stores file text plus source path.
- `BinaryResource`: stores raw bytes plus source path.

Built-in file helpers:

- `FileSystem::exists(path)`
- `FileSystem::readTextFile(path)`
- `FileSystem::readBinaryFile(path)`
- `AssetLoaders::loadText(resources, id, path)`
- `AssetLoaders::loadBinary(resources, id, path)`
- `AssetLoaders::loadKeyValuePrefab(resources, id, path)`

### Prefabs

`EntityPrefab` is the in-memory prefab model. It is independent of the source format, so JSON or YAML loaders can be added later without changing world instantiation.

Current prefab support:

- `EntityPrefab`
- `PrefabDocument`
- `PrefabInstantiator::instantiate(world, prefab)`
- `PrefabInstantiator::instantiateAll(world, document)`
- `KeyValuePrefabLoader::loadFromText(text)`
- `KeyValuePrefabLoader::loadFromFile(path)`

Current built-in prefab components:

- `HealthComponent`
- `AttackComponent`
- `PositionComponent`

Example key/value prefab format:

```text
[Player]
health.current=120
health.max=120
attack.damage=25
position.x=0
position.y=0
```

### Scene Management

`SceneManager` maps scene names to scene factories. It can create scenes on demand and replace the active scene in `Application`, which gives the engine a clean path toward menus, levels, loading screens, and editor previews.

### Demo

`demo/main.cpp` wires the pieces together through the runtime architecture:

1. Creates an `Application`.
2. Registers `CombatDemoScene` through `SceneManager`.
3. Loads a key/value prefab document through `AssetLoaders` and `ResourceManager`.
4. Instantiates entities and components through `PrefabInstantiator`.
5. Runs combat through a scheduled `ISystem`.
6. Publishes combat log events through `EventBus`.
7. Displays state through `RenderSystem`.

## Future Improvements

- Add movement and input systems.
- Add better test coverage for systems and world/component contracts.
- Publish combat results through events instead of string-only return values.
- Add structured-data loaders for scene descriptions and gameplay definitions.
- Add JSON/YAML prefab loaders behind the same `EntityPrefab` model.
- Add serialization for saving and loading simple game state.
- Add resource, scene, and module boundaries before scaling into rendering/audio/physics layers.
- Expand the demo into a small turn-based encounter.
