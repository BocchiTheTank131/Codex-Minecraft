# Agent guide: Voxel Frontier

## Scope and working rules

This file guides work throughout this repository. Follow the user's current task
and any more specific instructions in a subdirectory. Inspect `git status`, the
current source and relevant validation reports before editing. Preserve unrelated
tracked and untracked work; never reset, stash, discard or overwrite it to obtain a
clean tree. Do not treat `work/` prototypes as the production implementation.

Use the local working tree as the source of truth, including unpublished changes.
At the time this guide was written, the released version is **v2.20.1: Village Polish &
Living World Improvements**. Confirm the actual version in `CMakeLists.txt` rather than assuming
this statement remains current.

Do not commit, push, tag, publish, create a release, install the game or modify real
saved worlds without explicit user authorization for that work. Prior publication
approval does not authorize publication of later changes. Local builds and
disposable test profiles are the normal development workflow.

## Project and source map

The production game uses C++17, OpenGL 3.3 and CMake. Public declarations live in
`include/`, implementation in `src/`, runtime art in `assets/`, validation and
release documentation in `docs/`, and Windows installer sources in `packaging/`.
The root `CMakeLists.txt` lists production translation units and dependency pins.

| Area | Main entry points |
| --- | --- |
| Game loop, controls and interaction | `Game.cpp`, `InputManager.cpp`, `GameBuilding.cpp`, `GameAdventure.cpp`, `GameVillages.cpp` |
| Terrain, versions and structures | `World.cpp`, `WorldDensity.cpp`, `WorldGeography.cpp`, `WorldAesthetics.cpp`, `WorldDiversity.cpp`, `Structure.cpp`, `UndergroundStructures.cpp` |
| Streaming and rendering | `WorldStreaming.cpp`, `ChunkStreaming.cpp`, `Renderer.cpp`, terrain mesh/LOD/occlusion modules |
| Inventory, crafting and UI | `Inventory` declarations/implementation, `InventoryUI.cpp`, `QualityOfLife.cpp`, `Definitions.cpp`, `Building.cpp` |
| Survival and NPC behavior | `Survival.cpp`, `VillageBehavior.cpp`, `Villages.cpp`, `include/Villages.h`, `include/VillageUi.h` |
| Exploration and navigation | `Adventure.cpp`, `AdventureChambers.cpp`, `ExplorationLoot.cpp`, `Navigation.cpp` |
| Machinery | `Automation.cpp`, existing block states, containers and farming systems |
| Persistence | `SaveFile.cpp`, `SaveSnapshot.cpp`, world/entity serialization and Game checkpoint coordination |
| Regression harness | `tests/stability/`, `tests/fixtures/`, `tools/` validation scripts |

Read the matching declarations and call sites before changing a subsystem. Reuse
existing transactions, input routing, scaled UI, registries, geometry and save
coordination instead of adding competing implementations.

## Compatibility invariants

- Preserve **Generators V1–V24 byte-for-byte**, including unexplored chunks, loot,
  seed/RNG consumption and chunk-order independence. No new generator or terrain
  change is implied by a gameplay task. Existing worlds keep their saved version.
- Preserve stable 16-bit block/item IDs, block-state properties, compact terrain
  storage, historical texture assignments and the existing item-preview mappings
  (365 items at v2.20.0). Do not infer category/table membership from an open-ended
  item-ID range; validate explicit bounds when appending registry entries.
- Leave the four original hostile billboard mobs unchanged, including art, AI,
  spawning, movement, animation and combat, unless explicitly requested.
- Preserve checkpoint manifests, checksums, atomic commit, verified previous
  checkpoint recovery, session locks and failure backpressure. Keep AUT1, BDP1,
  ERL1, ENC1, VLG1, VIN1 and other existing records compatible.
- Background save encoders consume coherent immutable snapshots, never mutable
  live-world pointers. New persistent state must join the coordinated checkpoint;
  do not write an independent gameplay-state sidecar. Validate malformed data
  before mutating live state. Report Saved only after the complete commit succeeds.
- Every ownership transfer must either succeed completely or preserve the
  original items/state. Retain counts, durability, map identity and metadata.
  Respect hotbar locks, equipment, cursor stacks, container rules, drop admission
  and bounded checkpointed deferred-drop recovery. Do not consume outputs/rewards
  before capacity is accepted; claim flags must agree with inventory/world edits.
- Keep automation deterministic and bounded, NPC work loaded-terrain gated, and
  OpenGL calls on the graphics thread. Avoid synchronous distant generation in
  hints, discovery, navigation or UI queries. Maintain OpenGL 3.3 support.
- Preserve full-game persistence and **standalone-demo statelessness**. The known
  large-coordinate float-precision limitation is not an invitation for a partial
  coordinate migration.

## Build and package workflow

Use PowerShell-compatible commands from the repository root. Check installed
tools first. Current packaging defaults to Visual Studio 18 2026 x64; select an
installed supported generator rather than assuming it exists. Dependencies are
fetched by CMake. Windows standalone builds require MSVC; installer packaging
requires Inno Setup 7.

```powershell
# Full persistent game, with embedded assets/static runtime
cmake -S . -B build/installed-release -G "Visual Studio 18 2026" -A x64 -DVOXEL_STANDALONE=ON -DVOXELFRONTIER_STANDALONE_DEMO=OFF
cmake --build build/installed-release --config Release --parallel 4

# Stateless standalone demo
cmake -S . -B build/standalone-demo-release -G "Visual Studio 18 2026" -A x64 -DVOXEL_STANDALONE=ON -DVOXELFRONTIER_STANDALONE_DEMO=ON
cmake --build build/standalone-demo-release --config Release --parallel 4

# Build both and create local installer/demo/checksums; does not publish/install
.\package_release.ps1 -BuildJobs 4
```

Production binaries are under each build directory's `Release/`. Packaging writes
versioned `Windows-Setup.exe`, `Windows-Standalone.exe` and `SHA256SUMS.txt` into
`dist/`. Do not upload build directories, caches, developer profiles or unrelated
attachments as release assets. Verify checksums and embedded assets before an
authorized publication.

`assets/inventory_previews.json` is required: packaging rejects missing or stale
input/output fingerprints. `tools/generate_item_thumbnails.py` owns the offline
preview pipeline; do not enlarge old low-resolution icons or bypass checks. Asset
changes must retain correct registry mapping and avoid atlas bleeding/clipping.

## Safe testing

Use fresh disposable directories under `build/` for evidence and profiles. Set
`VOXEL_FRONTIER_DATA_DIR` to the isolated profile and create its
`.voxel_test_sandbox` marker before launching gameplay tests. Restore the previous
environment value afterward. Never point tests at the installed game or real
player saves. Prefer existing runners that already establish isolation.

The stability harness links production Release object files, excluding `main.obj`.
Rebuild production first after source edits, then rebuild the driver; stale object
files or mismatched full/demo objects invalidate results. See
`tests/stability/CMakeLists.txt` for `BASE`, `DEPENDENCIES` and `DEMO` options.

```powershell
# Existing broad runner; inspect script/profile handling before use
.\tools\run_stability_regressions.ps1 -IncludeDemo

# v2.20.0 runners: require matching full/demo drivers and fresh output paths
python tools/run_village_validation.py --phase focused --output build/agent-village-focused
python tools/validate_villages_v2200.py --phase functional --output build/agent-functional
python tools/validate_villages_v2200.py --phase generation --output build/agent-generation
python tools/validate_villages_v2200.py --phase release --output build/agent-release
```

Version-specific runners may hardcode executable paths, versions or evidence
directories. Inspect them before reuse; several refuse existing output paths.
Add focused reproductions for bugs, run affected checks while iterating, then
perform broader verification when stable. Do not repeatedly run the whole matrix
for minor edits or weaken tests to get a pass.

Compatibility evidence includes 1,326 historical hash records, 495 exact V23
fixtures per variant and 738 exact V24 fixtures per variant. Frozen archives live
in `tests/fixtures/`; never regenerate expected baselines from a changed candidate
to hide an output difference. Preserve existing automation, checkpoint, ownership,
farming, building, exploration, inventory and full/demo suites.

UI/world visual changes require actual native screenshots and inspection; passing
assertions alone does not establish visual quality. Compare performance with the
same seed, scene, settings, resident geometry and hardware; report frame-time
percentiles, memory and variation honestly. Distinguish constrained CPU proxies
from real low-end hardware and measured results from untested assumptions.

## Documentation to consult

Start with `README.md` and the report for the subsystem being changed:

- `docs/VILLAGES_LIVING_WORLD_VALIDATION_v2.20.0.md`: village identities, stock,
  routines, navigation limits, requests, projects, persistence and test evidence.
- `docs/EXPLORATION_RELICS_VALIDATION_v2.19.0.md` and
  `docs/V2190_REVIEW_FIXES.md`: exploration and ownership/interaction corrections.
- `docs/ENGINE_OPTIMIZATION_VALIDATION_v2.18.1.md`: immutable saving, streaming,
  GPU retirement and measured limits.
- `docs/GENERATOR_V23_VALIDATION.md`: generation stages and historical preservation.
- `docs/LOGIC_AUTOMATION_VALIDATION.md` and
  `docs/V2170_PRERELEASE_FIX_VALIDATION.md`: circuits, timing, atomic movement and
  item conservation.
- `docs/CHECKPOINT_v2.14.3_VALIDATION.md`, `docs/STABILITY_VALIDATION.md`,
  `docs/QOL_VALIDATION_v2.16.2.md` and `docs/ICON_VISUAL_VALIDATION_v2.16.1.md`:
  checkpoint, UI, inventory and preview contracts.

Treat old reports as evidence for their recorded source/binaries, not proof of
the current tree. Record exact changes, commands, source/build fingerprints,
results and limitations for substantial updates. Keep release notes and README
consistent with delivered behavior. State what remains untested or deferred;
never claim all bugs are fixed because a suite passes.
