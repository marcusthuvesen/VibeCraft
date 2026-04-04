# VibeCraft Agent Guide

Read `README.md` before making any substantial change. It is the source of truth for architecture, workflow, product direction, validation expectations, and file-size policy. This file is only an agent-focused addendum.

## Start Here

- Read `README.md` first.
- Keep changes focused and local to the relevant domain folder under `include/vibecraft/` and `src/`.
- Prefer adding or extending focused modules over growing hotspot files.
- If architecture, workflow, or product direction changes, update `README.md` in the same task.
- Do not push or create commits unless explicitly asked.

## Build And Test

Use the repo presets and validation flow from `README.md` and `CMakePresets.json`.

### Default host flow

```sh
cmake --preset default
cmake --build --preset debug
ctest --preset debug --output-on-failure
build/default/bin/vibecraft
```

### Windows host flow

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
build/windows-debug/bin/vibecraft.exe
```

For meaningful gameplay changes, smoke-test startup, movement, focus-loss mouse release, world loading, mining, placing, and menus after the automated checks.

## Architecture Guardrails

- `app` orchestrates systems and frame flow. Do not dump new gameplay logic into `src/app/Application.cpp` by default.
- `world` owns authoritative voxel state, terrain, persistence, and world edits.
- `render` owns bgfx lifecycle, GPU resources, shaders, and presentation. It must not own gameplay or world state.
- `meshing` converts world data into renderable mesh data and should stay gameplay-agnostic.
- `multiplayer` changes should be deliberate and only expand protocol or sync state when the feature truly requires it.
- `core` is for logging and small shared helpers, not a dumping ground.

## Hotspots

Treat these files as deliberate-change areas. Avoid growing them unless the task clearly belongs there.

- `src/app/Application.cpp`
- `include/vibecraft/world/Block.hpp`
- `include/vibecraft/world/BlockMetadata.hpp`
- `include/vibecraft/render/Renderer.hpp`
- `README.md`

The repo policy from `README.md` still applies: treat `700` lines as an extraction warning and `800` lines as a hard limit.

## Behavior Preservation

Unless the task explicitly changes them, preserve current behavior for:

- movement
- mining
- placing
- crafting
- inventory and dropped items
- save/load behavior
- multiplayer compatibility

Be extra careful when touching block enums, serialization, player state, world persistence, or network protocol contracts.

## Assets, Atlas, And Shaders

- When you need block textures, item images, or related art that is not yet in this repo, fetch them from [PrismarineJS minecraft-assets, `data/1.10`](https://github.com/PrismarineJS/minecraft-assets/tree/master/data/1.10) (e.g. `blocks/`, `items/`, `colormap/`). Import into the project under `assets/` as appropriate, then wire through atlas or UI paths as the task requires. `README.md` still governs broader asset policy (including other placeholder sources).
- When changing files under `assets/textures/materials/`, check whether the atlas layout changed and run `scripts/build_chunk_atlas.sh` if needed.
- The atlas script states that tile order must match `BlockMetadata` tile indices. Keep those in sync.
- Shader edits under `assets/shaders/` require a rebuild and runtime validation because shader binaries are generated as part of the CMake build.
- When changing textures or shaders, verify the game visually instead of relying on tests alone.

## Change Style

- Prefer composition and small helpers over monolithic additions.
- Keep public headers focused and move implementation detail into `.cpp` files when possible.
- Add brief comments only where the logic is genuinely hard to parse quickly.
- Mirror the existing folder layout before inventing new top-level structure.

## Useful Reference Files

- `README.md`
- `CMakePresets.json`
- `CMakeLists.txt`
- `scripts/build_chunk_atlas.sh`
- `tests/`
