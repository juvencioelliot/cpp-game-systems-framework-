# GameCoreCPP

GameCoreCPP is a modular C++ game systems framework. It is not trying to clone Unity or Unreal feature-for-feature yet; it is building the kind of clean core those engines depend on: entity lifetime, component ownership, system-driven behavior, and a small but testable runtime layer.

The current demo runs a playable player-versus-enemy combat scene through the engine runtime. It renders live frames through SDL2 when available, with a terminal renderer as fallback.

Combat is sample gameplay, not an engine-core assumption. The engine core now focuses on generic ECS lifetime, scheduling, resources, prefabs, input actions, transforms, and backend-neutral rendering.

## Goals

- Demonstrate modern, readable C++ architecture.
- Keep entity, component, and system responsibilities separate.
- Use CMake so the project builds from the command line.
- Build a strong foundation that can evolve toward a larger engine-style runtime.
- Keep entity lifetime and component ownership centralized through `World`.

## Quick Tutorial: Build and Run

This project uses CMake and requires a C++17-capable compiler. SDL2 is optional and enables the windowed demo backend.

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

Run the visual combat demo:

```bash
./build/GameCoreDemo
```

The demo creates a player and enemy from a prefab, reads SDL keyboard input, runs movement and attack systems, and redraws a small arena with health bars and combat logs.

Controls in the SDL build:

- `WASD` or arrow keys: move
- `Space` or `Enter`: attack adjacent enemy
- `Escape` or window close: exit

### 5. Run the tests

After building, run the test suite with CTest:

```bash
ctest --test-dir build --output-on-failure
```

### 6. Run the benchmark

Benchmarks are optional:

```bash
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DCMAKE_DISABLE_FIND_PACKAGE_SDL2=TRUE -DGAMECORE_BUILD_BENCHMARKS=ON
cmake --build build-perf --target GameCoreRuntimeBenchmark
./build-perf/GameCoreRuntimeBenchmark
```

The current benchmark covers entity lifetime, component iteration, movement, event publishing, and draw-frame building.

## Documentation

- `README.md`: project entry point and high-level current architecture.
- `FRAMEWORK_OVERVIEW.md`: conceptual walkthrough of the runtime pieces.
- `FRAMEWORK_USAGE.md`: practical usage examples.
- `PROJECT_DESIGN.md`: design rationale, roadmap, and future agent handoff.
- `CODING_SESSION_CHANGES.md`: detailed record of the latest architecture session.

## Current Architecture

```text
GameCoreCPP/
├── CMakeLists.txt
├── CODING_SESSION_CHANGES.md
├── FRAMEWORK_OVERVIEW.md
├── FRAMEWORK_USAGE.md
├── PROJECT_DESIGN.md
├── README.md
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
│       │   ├── MoveIntentComponent.h
│       │   ├── NameComponent.h
│       │   ├── PositionComponent.h
│       │   ├── ProgressComponent.h
│       │   ├── RenderableComponent.h
│       │   └── TransformComponent.h
│       └── Systems/
│           ├── MovementSystem.h
│           ├── RenderSystem.h
│           └── TransformSystem.h
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
│       ├── MovementSystem.cpp
│       ├── RenderSystem.cpp
│       └── TransformSystem.cpp
├── benchmarks/
│   └── runtime_benchmark.cpp
├── demo/
│   ├── CombatGameplay.cpp
│   ├── CombatGameplay.h
│   └── main.cpp
└── tests/
    ├── basic_tests.cpp
    └── world_tests.cpp
```

### Core

- `Application` owns the active scene and drives deterministic frame updates.
- `ApplicationContext` is the limited interface scenes use to request app-level services.
- `AssetLoaders` provides file-backed resource loader helpers for built-in resource types.
- `Diagnostics` provides level-based logging with configurable sinks.
- `EntityPrefab` defines a format-neutral prefab data model and instantiation helpers.
- `Entity.h` defines `EntityID`, the lightweight generation-aware identifier used to represent game objects.
- `EntityManager` creates, destroys, tracks, and recycles entity IDs while rejecting stale recycled handles.
- `ComponentStorage<T>` is a small generic storage class backed by `std::unordered_map<EntityID, T>`.
- `EventBus` provides typed publish/subscribe messaging for decoupled runtime communication.
- `FileSystem` provides basic text and binary file reads.
- `InputManager` tracks backend-agnostic action states across frames.
- `KeyValuePrefabLoader` parses simple prefab files into the prefab data model.
- `ResourceManager` loads, caches, retrieves, reloads, and unloads typed resources by string ID.
- `Scene` groups a `World`, a `SystemScheduler`, and lifecycle hooks into a runtime unit.
- `SceneManager` registers named scene factories and switches the active application scene.
- `StateMachine<StateID>` provides reusable state lifecycle and transition handling for gameplay, AI, UI, and runtime modes.
- `FrameContext` carries delta time, total time, frame index, and fixed-frame index data into runtime systems.
- `ISystem` defines the update contract for systems that run every frame.
- `SystemScheduler` owns runtime systems and updates them in explicit registration order.
- `World` is the main runtime facade. It owns entity lifetime, creates component storages on demand, supports immediate and deferred destruction, removes an entity's components when that entity is destroyed, and exposes the event bus.

Scene updates run in a clear order: before-systems hooks, scheduled systems, deferred destruction flush, then after-systems hooks.

### Components

Components are simple data structs:

- `PositionComponent` stores a simple grid position.
- `MoveIntentComponent` stores requested grid movement for the next movement update.
- `ProgressComponent` stores generic current/max progress for rendering bars.
- `NameComponent` stores a display/debug name.
- `RenderableComponent` stores generic visibility and glyph/layer render data.
- `TransformComponent` stores local transform data.
- `WorldTransformComponent` stores resolved world transform data.

Combat-specific health and attack components live in the demo gameplay layer, not the engine core.

### Systems

Systems contain behavior:

- `MovementSystem` applies movement intent to positions.
- `TransformSystem` resolves parented local transforms into world transforms.
- `RenderSystem` builds backend-neutral draw commands from renderable component data and sends them to a rendering backend. The current backends are SDL2 and terminal.
- Runtime systems can implement `ISystem` and be driven by `SystemScheduler`.

### State Machines

The framework provides a generic `StateMachine<StateID>`. It supports registered states, enter/update/exit callbacks, current-state tracking, optional transition rules, and clearing. The engine provides the mechanism; game projects define their own state IDs and behavior.

### Scenes

`Scene` is the runtime unit that groups a `World` and `SystemScheduler`. Scenes can be initialized, updated, and shut down, with overridable lifecycle hooks for game-specific setup and teardown.

### Application

`Application` owns the active scene, shared resources, input state, and frame updates with `FrameContext`. It can replace scenes, stop the frame loop, and track the current frame index and accumulated runtime.

### Input

`InputManager` tracks named actions such as `PrimaryAction`, `Confirm`, or `MoveUp`. It is backend-agnostic: platform code can set action states, and scenes/systems can query `wasPressed`, `isHeld`, and `wasReleased`.

When SDL2 is available, `SdlInputBackend` maps keyboard and window events into those actions. Other platform, controller, terminal, or scripted input backends can feed the same `InputManager` contract later.

### Resources

`ResourceManager` is a type-safe cache for engine and game data. It stores resources by type and string ID, uses caller-provided loader functions, and returns `ResourceHandle<T>` values backed by `std::shared_ptr`.

Current behavior:

- Loading the same type and ID twice returns the cached resource.
- Different resource types can use the same ID without colliding.
- Resources can be queried, required, unloaded, cleared, and reloaded.
- Reloading replaces the cached resource for future lookups; existing handles remain valid references to the older resource.
- `Application` owns a shared resource manager so resources can survive scene changes.
- Resource metadata tracks ID, type name, source path, load count, reload count, and last reload error.
- `ResourceManager::events()` publishes `ResourceLoadedEvent` and `ResourceReloadFailedEvent`.

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
- `AssetLoaders::loadKeyValuePrefab(resources, id, path, registry)`

### Prefabs

`EntityPrefab` is the in-memory prefab model. It is independent of the source format, so JSON or YAML loaders can be added later without changing world instantiation.

Current prefab support:

- `EntityPrefab`
- `PrefabDocument`
- `PrefabComponentRegistry`
- `PrefabInstantiator::instantiate(world, prefab)`
- `PrefabInstantiator::instantiateAll(world, document)`
- `KeyValuePrefabLoader::loadFromText(text)`
- `KeyValuePrefabLoader::loadFromText(text, registry)`
- `KeyValuePrefabLoader::loadFromFile(path)`

Current built-in prefab components:

- `PositionComponent`
- Custom components through `PrefabComponentRegistry`

Example key/value prefab format:

```text
[Player]
position.x=0
position.y=0
```

The combat demo registers `health.*` and `attack.*` prefixes through `PrefabComponentRegistry`, which shows how game-specific data can stay outside the engine core.

### Scene Management

`SceneManager` maps scene names to scene factories. It can create scenes on demand and replace the active scene in `Application`, which gives the engine a clean path toward menus, levels, loading screens, and editor previews.

### Demo

`demo/main.cpp` wires the pieces together through the runtime architecture:

1. Creates an `Application`.
2. Registers `CombatDemoScene` through `SceneManager`.
3. Loads a key/value prefab document through `AssetLoaders` and `ResourceManager`.
4. Instantiates entities and components through `PrefabInstantiator`.
5. Runs movement and demo-owned combat intent through scheduled `ISystem` implementations.
6. Publishes combat log events through `EventBus`.
7. Displays generic `RenderableComponent`, `NameComponent`, and `ProgressComponent` data through `RenderSystem`.

## Future Improvements

- Add a real time/fixed-step layer with pause and manual stepping.
- Add tests for fixed-step catch-up, deterministic timing, and input-to-intent timing.
- Add deferred component add/remove command support.
- Add camera and sprite/render components on top of the draw-command path.
- Add structured-data loaders for scene descriptions and gameplay definitions.
- Add JSON/YAML prefab loaders behind the same `EntityPrefab` model.
- Add serialization for saving and loading simple game state.
- Add pooled or archetype-style storage if profiling shows component iteration needs it.
- Add resource, scene, and module boundaries before scaling into rendering/audio/physics layers.
- Expand rendering beyond glyph-backed demo draw commands once asset and scene contracts stabilize.
