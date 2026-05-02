# GameCoreCPP Coding Session Changes

This document records the architecture and cleanup work completed during the current coding session.

## Session Goal

The main goal was to make the framework less combat-oriented and more useful as a general game systems foundation. Combat now lives in the demo layer, while the engine core owns reusable runtime contracts: entity lifetime, component storage, system scheduling, transforms, input actions, resources, prefabs, events, and backend-neutral rendering.

## Engine Core Changes

### Entity Lifetime

- Added generation-aware `EntityID` encoding.
- Reworked `EntityManager` from a living-ID set into slot storage with recycled indices and generations.
- Stale recycled handles are now rejected by `isAlive(...)`.
- Added generation wraparound protection so encoded IDs remain valid inside the available generation bit range.
- Added an explicit max encodable entity index guard.

### Deferred Destruction

- Added `World::deferDestroyEntity(...)`.
- Added `World::flushDeferredDestruction(...)`.
- `Scene::update(...)` now flushes deferred destruction after scheduled systems and before after-systems hooks.
- Deferred destruction requests are deduplicated so the same entity is queued once.

### Scene Frame Sequencing

- Added `Scene::onBeforeSystems(...)`.
- Added `Scene::onAfterSystems(...)`.
- Kept the older `onUpdate(...)` path by calling it from the default `onAfterSystems(...)`.
- The frame order is now:

```text
onBeforeSystems
scheduled systems
flush deferred destruction
onAfterSystems
```

This lets scenes write input intent before simulation systems consume it in the same frame.

### Time Context

- Expanded `FrameContext` with:
  - `deltaSeconds`
  - `frameIndex`
  - `totalSeconds`
  - `fixedFrameIndex`
- `Application` now tracks accumulated runtime through `totalSeconds()`.

### System Scheduling

- `SystemScheduler` still supports phase, priority, and deterministic insertion-order ties.
- Sorting now happens only when systems are added instead of every frame.

### Generic Components

Added engine-owned generic components:

- `ProgressComponent`
- `NameComponent`
- `RenderableComponent`
- `TransformComponent`
- `WorldTransformComponent`

Combat-specific health, attack, and attack intent components were removed from the engine core.

### Transform System

- Added `TransformSystem`.
- It resolves local transforms into `WorldTransformComponent`.
- It supports parent/child transform chains.
- Cyclic parent relationships fall back to local transforms instead of accumulating through the cycle.

### Rendering

- Replaced entity-state rendering with backend-neutral draw commands:
  - `DrawFrame`
  - `DrawCommand`
  - `DrawCommandType`
- `IRenderBackend` now renders `DrawFrame`.
- `RenderSystem` can build draw commands from generic render components.
- Existing label-based rendering remains available for compatibility.
- Terminal and SDL render backends consume draw commands.
- Render text now uses generic progress wording instead of combat-specific HP wording.
- The component-driven render path reserves draw-command capacity before building frames.

### Prefabs And Assets

- Core prefabs now include only built-in position data.
- Added `PrefabComponentRegistry` support to `AssetLoaders::loadKeyValuePrefab(...)`.
- Game-specific prefab fields such as `health.*` and `attack.*` are registered by the demo gameplay layer.

## Demo Changes

- Moved combat code into `demo/CombatGameplay.h` and `demo/CombatGameplay.cpp`.
- Demo-owned combat now contains:
  - `HealthComponent`
  - `AttackComponent`
  - `AttackIntentComponent`
  - `CombatSystem`
  - `AttackIntentSystem`
  - combat events
- `demo/main.cpp` now creates generic engine render data with `NameComponent`, `RenderableComponent`, and `ProgressComponent`.
- SDL input maps `Space` and `Enter` to the generic `PrimaryAction` action.
- The demo interprets `PrimaryAction` as attack, keeping that gameplay meaning outside the engine.

## Benchmarking

- Added optional benchmark target behind `GAMECORE_BUILD_BENCHMARKS`.
- Added `benchmarks/runtime_benchmark.cpp`.
- Current benchmark areas:
  - entity create/add/destroy
  - `World::each` component iteration
  - `MovementSystem`
  - `EventBus`
  - draw-frame building

The benchmark identifies `buildDrawFrame` as the current primary runtime hotspot.

## Tests Added Or Updated

Added or updated tests for:

- Generation-aware entity recycling.
- Generation wraparound safety.
- Deferred destruction flushing.
- Deferred destruction deduplication.
- Scene before-systems and after-systems ordering.
- Same-frame input intent consumption.
- Transform hierarchy resolution.
- Transform cycle fallback.
- Component-driven render frame generation.
- Invisible renderables being skipped.
- Expanded `FrameContext` data.

## Documentation Updated

Updated:

- `README.md`
- `FRAMEWORK_OVERVIEW.md`
- `FRAMEWORK_USAGE.md`
- `PROJECT_DESIGN.md`

The docs now describe the current generic engine architecture, demo-owned combat layer, draw-command rendering, generation-aware entities, deferred destruction, transform system, benchmark target, and next work.

## Verification

The session was verified with:

```bash
git diff --check
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build-perf --target GameCoreRuntimeBenchmark
./build-perf/GameCoreRuntimeBenchmark
```

The terminal demo was also smoke-tested through a headless SDL-disabled build.

## Current Known Limitations

- The runtime loop still uses a simple fixed-delta model.
- There is no fixed-step accumulator, pause state, or manual stepping API yet.
- Deferred mutation only covers entity destruction, not component add/remove commands.
- Rendering is still glyph/grid/progress oriented rather than asset-backed sprite/camera rendering.
- The demo AI and win/loss flow are still simple and partly scene-owned.
- Prefabs need stronger validation and richer structured-data loaders.
- Tests are improving, but broad test files should continue to split by subsystem.

## Recommended Next Step

The best next architecture step is the time/fixed-step foundation:

1. Add a small `Clock` or `Time` abstraction.
2. Add fixed-step simulation support without breaking `runFrames(...)`.
3. Add pause and manual stepping if the API stays clean.
4. Add focused tests for timing, fixed-step catch-up, deterministic frame indices, and input-to-intent timing.

