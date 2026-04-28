# GameCoreCPP — Project Design Document

## 1. Project Overview

GameCoreCPP is a modular C++ game systems framework designed to explore the foundations of game architecture.

The goal is not to build a full game engine, but to create a small, clean, and extensible framework that demonstrates core game development concepts such as:

- Entity management
- Component-based design
- System-driven logic
- Game loop architecture
- Event handling
- Separation of data and behavior

A small generated visual demo runs on top of the framework to prove that the runtime pieces work together. The demo currently produces an HTML report with embedded local SVG assets because the engine does not yet have a real rendering backend.

---

## 2. Main Goal

The main goal of this project is to demonstrate strong C++ and software architecture skills through a reusable game systems framework.

This project should show that the developer understands:

- How game objects can be represented as entities
- How data can be separated into components
- How systems process entities with specific components
- How a game loop coordinates input, update, and rendering
- How to structure a C++ project cleanly
- How to write code that can grow over time

---

## 3. Design Philosophy

The framework should stay simple but intentional. A large engine is built from boring, reliable contracts: entities have clear lifetimes, components have clear owners, systems have narrow behavior, and the runtime decides how pieces are wired together.

The project should prioritize:

- Clean code over complexity
- Readability over cleverness
- Small reusable systems
- Clear separation of responsibility
- Incremental development
- Good commit history
- Testable engine primitives before large feature additions

This is not meant to compete with Unreal Engine or Unity. It is a learning and portfolio project focused on architecture.

---

## 4. Core Architecture

### Entities

An entity represents a game object.

Examples:

- Player
- Enemy
- NPC
- Projectile
- Item

At first, an entity can simply be an integer ID.

### World

`World` is the runtime owner of entity and component state.

Responsibilities:

- Create and destroy entities
- Lazily create typed component storages
- Add, remove, query, and fetch components
- Remove all components for an entity when that entity is destroyed
- Provide systems with the component storages they need
- Provide access to runtime events

The purpose of `World` is to prevent gameplay code from manually coordinating loose entity and component containers. That gives the project a more scalable foundation for scenes, systems, serialization, and editor/runtime tooling later.

### Components

Components are plain data objects. They should not own behavior or know which systems use them.

Current components:

- `HealthComponent`
- `AttackComponent`
- `PositionComponent`

### Systems

Systems contain behavior and operate on component data. They should stay narrow and testable.

Current systems:

- `CombatSystem`
- `RenderSystem`

Runtime systems that need to run every frame can implement `ISystem`. `SystemScheduler` owns those systems and updates them in registration order using a `FrameContext`.

`FrameContext` currently carries:

- Delta time in seconds
- Frame index

This gives the project a clear place to add fixed updates, render phases, priorities, pause state, and profiling later.

### Events

`EventBus` provides typed publish/subscribe messaging. Systems can publish small event structs and listeners can subscribe without creating direct dependencies between systems.

Examples of future events:

- Damage applied
- Entity defeated
- Scene loaded
- Input action pressed
- Resource loaded

### State Machines

`StateMachine<StateID>` is a generic state lifecycle utility. It is intentionally not tied to specific gameplay concepts such as idle, attack, patrol, or dead. Game projects define those states themselves.

Responsibilities:

- Register available states
- Track the current state
- Run enter, update, and exit callbacks
- Allow optional transition rules
- Reject unknown states
- Clear state definitions and transition rules

This can support AI behavior, animation flow, UI screens, turn phases, runtime mode, editor mode, pause state, and loading flow without forcing a specific game design into the engine.

### Scenes

`Scene` groups runtime state into a loadable and updateable unit.

Responsibilities:

- Own a `World`
- Own a `SystemScheduler`
- Initialize scene-specific content
- Update scheduled systems
- Run scene-specific update hooks
- Shut down runtime-owned systems and event listeners

Scenes provide the foundation for levels, menus, battle encounters, editor previews, loading screens, and tests that need a complete runtime unit.

### Application

`Application` owns the active scene, owns shared resources, owns input state, and drives frame updates.

Responsibilities:

- Set or replace the active scene
- Shut down the previous scene before replacement
- Drive scene updates with `FrameContext`
- Provide access to shared resources
- Provide access to input state
- Track frame index
- Stop the frame loop on request

The current application loop is deterministic and test-friendly. It does not yet own platform windows, real-time clocks, input devices, or rendering backends.

Scenes access application-level services through `ApplicationContext`, a narrow interface exposed by `Scene::application()`. This lets scenes request shutdown or access shared resources without depending on global state.

### Input

`InputManager` stores backend-agnostic action state.

Responsibilities:

- Set named actions up or down
- Track held actions
- Track actions pressed this frame
- Track actions released this frame
- Advance input frame state deterministically
- Clear all known actions

The engine does not yet read keyboard, controller, terminal, or platform events directly. Those backends can later translate raw input into action names.

### Scene Management

`SceneManager` registers scene factories by name and creates scenes on demand.

Responsibilities:

- Register named scene factories
- Remove registered scenes
- Create scenes by name
- Ask `Application` to switch to a named scene
- Reject empty scene factories

Scene management keeps scene construction out of the core application loop and gives game code one place to define available levels, menus, loading screens, and editor previews.

### Resources

`ResourceManager` is a typed cache for assets and data. It is intentionally independent of rendering, audio, physics, and file formats so those systems can be added without rewriting the ownership model.

Responsibilities:

- Load resources by type and string ID
- Cache repeated loads
- Return stable `ResourceHandle<T>` values
- Keep different resource types isolated even when IDs match
- Retrieve optional resources
- Require resources with an exception on missing IDs
- Unload individual resources
- Clear resources by type or globally
- Reload resources through their stored loader

Important behavior:

- Resource IDs are non-empty strings.
- Loaders return `std::shared_ptr<T>` so ownership is explicit.
- `loadValue<T>` exists for simple value construction.
- Reloading updates the cached resource used by future lookups.
- Existing handles remain valid because they keep their own `std::shared_ptr`.
- `Application` owns a resource manager so common resources can survive scene changes.

Future resource work should add concrete loader helpers for file-backed text, structured data, textures, audio, materials, and scene descriptions.

### File and Asset Loading

`FileSystem` is the lowest-level file IO helper.

Responsibilities:

- Check whether a file exists
- Read a full text file
- Read a full binary file
- Throw clear exceptions when files cannot be opened

`AssetLoaders` sits above `FileSystem` and below game-specific content systems. It converts files into typed resources and stores them in `ResourceManager`.

Built-in resource types:

- `TextResource`: text content and source path
- `BinaryResource`: raw bytes and source path

Current asset loader helpers:

- Load text resources by ID and path
- Load binary resources by ID and path
- Load key/value prefab documents by ID and path
- Preserve reload support through the `ResourceManager` loader contract

This gives the engine a real file-backed asset path without committing yet to image, audio, model, or structured-data libraries.

### Prefabs

`EntityPrefab` is the format-neutral model for data-driven entity creation.

Responsibilities:

- Store entity prefab data in memory
- Represent optional built-in components
- Instantiate one prefab into a `World`
- Instantiate a full prefab document into a `World`

Current prefab component support:

- `HealthComponent`
- `AttackComponent`
- `PositionComponent`

`KeyValuePrefabLoader` is the first concrete prefab format loader. It parses simple section-based `key=value` text into `PrefabDocument`.

Example:

```text
[Player]
health.current=120
health.max=120
attack.damage=25
position.x=0
position.y=0
```

The engine runtime should depend on `EntityPrefab` and `PrefabDocument`, not on the key/value parser. This keeps the path open for future `JsonPrefabLoader` or `YamlPrefabLoader` implementations that produce the same in-memory model.

---

## 5. Current Demo

The combat demo is hosted by the engine runtime rather than manually wiring everything in `main`.

The demo currently uses:

- `Application`
- `SceneManager`
- `Scene`
- `World`
- `SystemScheduler`
- `ISystem`
- `EventBus`
- `InputManager`
- `ResourceManager`
- `AssetLoaders`
- `EntityPrefab`
- `CombatSystem`

`CombatDemoScene` creates the combat entities during scene initialization from a prefab document loaded through `AssetLoaders` and cached by `ResourceManager`. A scheduled combat-round system advances one round per frame, publishes log events, records the combat state, and asks the application to stop when combat is complete.

The current demo writes `build/GameCoreVisualDemo.html`. That report embeds locally generated SVG assets for the player, enemy, and arena. This is useful for showing the framework working with assets, but it is still not a real renderer. The next engine work should treat the HTML demo as a showcase artifact, not as the rendering architecture.

---

## 6. Next Coding Session Roadmap

The next session should focus on hardening the runtime foundation before building more gameplay on top. The framework has enough pieces to prove the direction, but it is still a prototype foundation rather than a true engine foundation. The next work should turn the existing pieces into stronger contracts.

### 1. Runtime Loop And Time Foundation

Current state:

- `Application` can run deterministic frame updates.
- `FrameContext` carries delta time and frame index.
- There is no clock abstraction, fixed update accumulator, pause state, or update phase model.

Next work:

- Add a `Clock` or `Time` layer.
- Add clear time fields to `FrameContext`, such as:
  - `deltaTime`
  - `fixedDeltaTime`
  - `totalTime`
  - `frameIndex`
  - `fixedFrameIndex`
- Split system execution into explicit phases:
  - `Input`
  - `FixedUpdate`
  - `Update`
  - `LateUpdate`
  - `Render`
- Add deterministic fixed-step behavior with an accumulator.
- Keep a test-friendly manual stepping mode.
- Add tests for frame progression, fixed-step catch-up, zero-frame behavior, pause behavior, and deterministic stepping.

Goal:

- Make timing and update order reliable enough for physics, animation, AI, input, and rendering.

### 2. Rendering Abstraction

Current state:

- The visual demo writes HTML.
- There is no engine-owned `Renderer`.
- `RenderSystem` is still a simple placeholder concept, not a backend abstraction.

Next work:

- Add a small renderer-facing interface before choosing a platform backend.
- Start with 2D concepts:
  - sprites
  - draw commands
  - camera/view transform
  - draw layers or sorting order
  - viewport size
- Keep rendering backend-agnostic.
- Let systems submit render commands rather than write directly to output.
- Keep the HTML report separate as a demo/export tool until a real backend exists.

Goal:

- Define how the engine talks about drawing before tying it to SDL, SFML, OpenGL, Vulkan, Metal, or another implementation.

### 3. Transform Hierarchy

Current state:

- `PositionComponent` exists.
- There is no real transform model.
- No parent/child hierarchy exists.

Next work:

- Add `TransformComponent` with local position, rotation, and scale.
- Add parent/child relationship support or a companion hierarchy component.
- Add a `TransformSystem` that computes world transforms from local transforms.
- Decide how this relates to the existing `PositionComponent`.
- Add tests for parent movement, child world transforms, reparenting, and entity destruction.

Goal:

- Build a foundation for rendering, physics, collision, cameras, UI, and scene graph behavior.

### 4. ECS Maturity

Current state:

- Entities and components work.
- Component storage is simple and testable.
- Querying and lifecycle semantics are still basic.

Next work:

- Add entity generations or handles to prevent stale entity references.
- Add safer component access patterns.
- Add component queries/views for systems.
- Add deferred entity destruction so systems can safely iterate while entities are being removed.
- Add tests around invalid entities, destroyed entities, component removal, and iteration safety.

Goal:

- Make the ECS safe enough to support larger systems without accidental lifetime bugs.

### 5. Serialization And Data Formats

Current state:

- Prefabs are format-neutral in memory.
- A key/value parser exists as the first concrete format.
- JSON/YAML can be added later by targeting the same `PrefabDocument` model.

Next work:

- Keep key/value support for now.
- Add a serialization boundary that can later support JSON/YAML without changing runtime code.
- Improve prefab validation and error messages.
- Add versioning fields to asset documents before formats multiply.
- Consider adding a `JsonPrefabLoader` only after the runtime model settles.

Goal:

- Make data-driven content reliable and format-flexible.

### 6. Resource Lifecycle

Current state:

- `ResourceManager` caches typed resources.
- Text, binary, and prefab loading exist.
- Reload support exists at a basic level.

Next work:

- Add dependency tracking between resources.
- Add missing-resource behavior and placeholder/fallback policy.
- Add asset metadata.
- Improve reload behavior and diagnostics.
- Add typed loader registration instead of only helper functions.

Goal:

- Move from simple resource cache to a real asset lifecycle layer.

### 7. State Machines As First-Class Runtime Tools

Current state:

- A generic `StateMachine<StateID>` utility exists.
- It is not deeply integrated into application, scene, entity, or AI flow yet.

Next work:

- Keep the generic utility engine-owned.
- Add examples/tests for:
  - application mode
  - scene flow
  - entity AI
  - turn phases
- Avoid forcing game-specific states into the engine.

Goal:

- Give game developers a reliable state tool without dictating their gameplay model.

### 8. Scripting And Gameplay Extension Layer

Current state:

- Gameplay behavior is written in C++ systems and scenes.
- There is no scripting or behavior component layer.

Next work:

- First add clean C++ behavior extension points.
- Consider scripting only after runtime, ECS, resources, and serialization are stable.
- Keep scripts out of the engine core until the boundary is clear.

Goal:

- Make game-specific behavior easy to add without modifying engine internals.

### 9. Diagnostics

Current state:

- Demo output writes directly to generated HTML or console.
- Tests use assertions.
- There is no engine logging layer.

Next work:

- Add a small `Logger` or `Diagnostics` utility.
- Support info, warning, error, and debug levels.
- Add assertion helpers for engine invariants.
- Add optional profiling scopes around systems.
- Route demo messages through diagnostics or events.

Goal:

- Make failures and runtime behavior easier to understand as the engine grows.

### 10. Real Interactive Demo

Current state:

- The current visual demo proves integration.
- It is still automated.
- It does not use a real renderer or real input backend.

Next work:

- After the runtime loop, transforms, and renderer boundary exist, build a small interactive prototype.
- Candidate prototype:
  - player can move
  - player can attack
  - enemy responds through a simple AI state machine
  - entities are prefab-loaded
  - systems handle movement, combat, rendering, events, and win/loss flow

Goal:

- Prove that the framework can support an actual small game loop, not only an automated showcase.

### Recommended Order

For the next coding session, the best order is:

1. Add the runtime time/clock/fixed-step foundation.
2. Split system execution into phases.
3. Add broad tests for timing, phase order, pause behavior, and deterministic stepping.
4. Start the transform hierarchy.
5. Define the renderer abstraction.
6. Keep the current visual HTML demo working as an integration smoke test.

If time is short, prioritize:

1. `Clock` / `Time`.
2. `FrameContext` expansion.
3. fixed update stepping.
4. phase-aware `SystemScheduler`.

---

## 7. Future Agent Handoff Prompt

Use this prompt if a future coding agent needs to continue the project without the full conversation history:

```text
You are working in a C++17 CMake project named GameCoreCPP at:
/Users/juvenciognacadja/Documents/GitHub/cpp-game-systems-framework-

The project is a game systems framework prototype aiming toward a strong Unity/Unreal-like foundation, but still intentionally small and clean. Do not turn it into a giant engine in one pass. Strengthen contracts incrementally.

Current architecture:
- Application owns scene flow, shared ResourceManager, InputManager, and frame execution.
- ApplicationContext is the narrow interface scenes use for app services.
- Scene owns a World and SystemScheduler.
- World owns entity/component state and an EventBus.
- ECS currently supports entities and components such as HealthComponent, AttackComponent, and PositionComponent.
- SystemScheduler runs ISystem instances using FrameContext.
- EventBus provides typed publish/subscribe messaging.
- ResourceManager caches typed resources and supports reload.
- FileSystem and AssetLoaders load text, binary, and key/value prefab documents.
- EntityPrefab/PrefabDocument/PrefabInstantiator are format-neutral prefab runtime pieces.
- StateMachine<StateID> exists as a generic utility.
- InputManager tracks named action held/pressed/released state.
- The demo currently generates build/GameCoreVisualDemo.html using local SVG assets in demo/assets.

Important current limitation:
The visual demo is only an HTML report/export. There is no real renderer yet. Treat it as an integration smoke test, not the rendering architecture.

Next priority:
Build the runtime loop and timing foundation.

Implement conservatively:
1. Add a Clock/Time abstraction suitable for deterministic tests.
2. Expand FrameContext with deltaTime, fixedDeltaTime, totalTime, frameIndex, and fixedFrameIndex as appropriate.
3. Add explicit system phases: Input, FixedUpdate, Update, LateUpdate, Render.
4. Update SystemScheduler so systems can be registered into phases while preserving deterministic order.
5. Update Application so it can run fixed-step simulation with an accumulator and still support test-friendly manual stepping.
6. Add pause behavior if it fits cleanly.
7. Keep existing public behavior working where possible.
8. Keep the visual HTML demo working.
9. Add broad tests for timing, phase order, fixed-step catch-up, zero-frame behavior, pause/manual stepping, and deterministic frame indexing.

Constraints:
- Use the existing code style and simple architecture.
- Prefer small engine primitives over large abstractions.
- Use apply_patch for manual edits.
- Do not revert unrelated user changes.
- Run cmake --build build and ctest --test-dir build --output-on-failure before finishing.
- If the demo is changed, run ./build/GameCoreDemo and confirm build/GameCoreVisualDemo.html is generated.

Good follow-up after timing:
- TransformComponent and TransformSystem with parent/child world transform calculation.
- Renderer interface with backend-agnostic 2D draw commands.
- ECS maturity: entity generations, query views, deferred destruction.
- Stronger prefab validation and future JSON/YAML loader boundary.
- Resource dependency metadata and diagnostics.
```

---

## 8. Previous Gameplay Roadmap

The items below are still useful, but they should come after the runtime foundation is stronger.

### 1. Input Backend Bridge

Current state:

- `InputManager` exists.
- It tracks named actions.
- No real keyboard, terminal, controller, or platform backend feeds it yet.

Next work:

- Add a simple terminal input adapter or scripted input adapter.
- Translate raw input into action names such as `Attack`, `Confirm`, `MoveUp`, and `MoveDown`.
- Keep the adapter separate from `InputManager`.
- Add tests for action mapping and frame transitions.

Goal:

- The demo or first game should react to input instead of advancing fully automatically.

### 2. Movement Foundation

Current state:

- `PositionComponent` exists.
- There is no movement component or movement system.

Next work:

- Add `VelocityComponent` or `MovementIntentComponent`.
- Add `MovementSystem`.
- Use `InputManager` to set movement intent.
- Update positions through a scheduled system.
- Add prefab support for any new movement-related component if needed.

Goal:

- Entities should be able to move through systems rather than direct scene code.

### 3. Event-Driven Combat Cleanup

Current state:

- `CombatSystem` returns strings.
- The demo wraps those strings in `CombatLogEvent`.
- Combat is not fully event-native yet.

Next work:

- Add explicit events such as:
  - `DamageAppliedEvent`
  - `EntityDefeatedEvent`
  - `CombatMessageEvent`
- Update combat/demo flow to publish structured events.
- Keep string formatting at the presentation/logging boundary.

Goal:

- Combat systems should produce structured facts, not only text.

### 4. Prefab Expansion

Current state:

- Prefabs support health, attack, and position.
- Key/value loading works.
- The in-memory `EntityPrefab` model is format-neutral.

Next work:

- Add prefab support for any new movement/input-related components.
- Add better validation for incomplete prefab component groups.
- Consider separating prefab parsing errors into clearer error types later.

Goal:

- Gameplay entities should remain data-driven as new components are added.

### 5. Scene Description Layer

Current state:

- Individual entities can be loaded from prefab documents.
- A scene itself is still C++-defined.

Next work:

- Add a simple scene description model that lists prefab files or entity definitions.
- Load a scene description through `AssetLoaders`.
- Instantiate scene contents inside `Scene::onInitialize`.

Goal:

- Scenes should eventually be mostly data-defined, with C++ used for behavior and custom orchestration.

### 6. Logging And Diagnostics

Current state:

- Demo output writes directly to `std::cout`.
- Tests use assertions.
- There is no engine logging layer.

Next work:

- Add a small `Logger` or `Diagnostics` utility.
- Support info/warning/error levels.
- Keep the backend simple, initially console output.
- Route demo messages through this layer or through events consumed by this layer.

Goal:

- Engine output should be consistent and easy to redirect later.

### 7. Test Organization

Current state:

- `tests/basic_tests.cpp` is broad and useful but becoming large.

Next work:

- Split tests by subsystem:
  - `entity_tests.cpp`
  - `world_tests.cpp`
  - `event_tests.cpp`
  - `resource_tests.cpp`
  - `prefab_tests.cpp`
  - `application_scene_tests.cpp`
  - `input_tests.cpp`
- Keep CTest running all test executables.

Goal:

- Keep test coverage broad while making failures easier to locate.

### 8. First Interactive Prototype

After the above pieces, build a small interactive prototype.

Candidate:

- A terminal/grid encounter.
- Player can move.
- Player can attack.
- Enemy can respond through a simple AI state machine.
- Entities are loaded from prefab data.
- Systems handle movement, combat, rendering/logging, and win/loss conditions.

Goal:

- Prove that the framework can support an actual small game loop, not only an automated demo.

### Recommended Order

For the next coding session, the best order is:

1. Split tests by subsystem if the session starts with cleanup.
2. Add input backend bridge.
3. Add movement component/system.
4. Make combat event-native.
5. Expand prefab support for new components.
6. Refactor the demo into the first interactive prototype.

If time is short, prioritize:

1. Input backend bridge.
2. Movement system.
3. Interactive demo update.
