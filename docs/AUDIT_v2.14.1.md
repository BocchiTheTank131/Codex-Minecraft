# Voxel Frontier v2.14.1 audit

Date: 2026-10-16. Local revision: `2951413` (Release v2.14.1). GitHub main's CMakeLists.txt was also retrieved and reports v2.14.1; this is not a claim that every remote file was compared byte for byte.

Scope: persistence, player initialization/physics, farming, coordinate handling, streaming, culling/LOD, entities, navigation and validation infrastructure. No production code fixes were made. This is a broad audit, not a guarantee that every defect has been found.

Evidence labels: **Reproduced** means an executable check observed the behavior. **Inspected** means the production source establishes the behavior but the full player scenario was not reproduced. **Reported measurement** comes from existing release validation, not a new benchmark. **Design limit** identifies an intentional limit or architecture tradeoff worth improving.

Priority: P1 protects saved progress and basic correctness; P2 addresses gameplay and frame pacing; P3 improves scaling, tooling and maintainability. Design limits are not all bugs.

## P1: protect saved progress and basic correctness

### 1. Failed sidecar loads silently become default state

**Reproduced, P1.** `src/Game.cpp:588-612` ignores return values from player, inventory and mob loading. This differs from terrain and navigation loading, which refuse to continue when an existing file is invalid. An invalid inventory file opens as an empty inventory without a warning and the next successful save overwrites it. The game-path harness reproduced both opening silently and overwriting the invalid bytes. Failed mob loading can similarly discard saved animals and drops.

Fix: distinguish absent legacy files from existing invalid files. Refuse writable loading of an invalid existing sidecar, preserve its bytes and provide recovery information. Load all required components before entering gameplay.

### 2. Saving while dead commits incompatible generations of world state

**Reproduced, P1.** `Game::saveAll`, `src/Game.cpp:667-681`, skips `saveWorld` when dead but still checkpoints inventory, player, mobs, navigation and metadata. Terrain, chest/furnace contents and position can remain from the previous save while items and entities advance. The game-path test saved an Air cell, changed it to Stone, added a diamond, killed the player and saved: reload contained the old Air cell and the new diamond. This can lose construction and enable duplication, depending on the intervening transfers.

Fix: save current terrain/container state even on death and choose a valid saved respawn position separately. Checkpoint every component from one coherent state.

### 3. Rejoining Survival restores full health and hunger

**Reproduced, P1.** `src/Game.cpp:589-590` loads player statistics and immediately calls `Player::setCreativeMode(false)`. That setter unconditionally assigns health and hunger to 20 (`src/Player.cpp:121-130`). Both a direct player roundtrip and the real Game load path reproduced it. Direct check: saved health 13/hunger 10 became 20/20 after mode initialization.

Fix: initialize mode before restoring statistics or separate mode flags from healing/reset behavior. Verify survival reload with non-default health and hunger.

### 4. Successful mob/drop saves can be unreadable by their own loader

**Reproduced, P1.** `src/Survival.cpp:2713-2852`: writer does not enforce the loader's 5,000-mob and 5,000-drop bounds. Summoning 5,001 cows and creating 5,001 drops each saved successfully and failed loading. A single 65-Stone drop also saved successfully and was rejected because the loader requires legal stack sizes. Normal gameplay reachability varies; the API and command paths do not enforce the mob count invariant. Combined with finding 1, failure can become silent loss.

Fix: share writer/loader invariants; split oversized drops, constrain admission or expand the format safely. Never report successful saving of state that cannot reload.

### 5. One broken checkpoint blocks the complete world list

**Reproduced, P1.** `src/WorldLibrary.cpp:20,59-70`: `read` throws on recovery failure and `list` lets the exception escape the directory loop. `Game::refreshWorlds` catches it around the whole listing. With one healthy world and one invalid checkpoint, listing failed entirely rather than returning the healthy entry. On a fresh menu this can hide all worlds; a previously populated menu may retain stale entries.

Fix: isolate errors per world, retain healthy entries and show an unavailable entry with its recovery reason. Do not bypass recovery for the damaged world.

### 6. Player physics treats unloaded terrain as empty space

**Reproduced, P1.** `src/World.cpp:3800-3806` returns Air for absent chunks. `Player::update` uses normal collision against that result without a loaded-terrain barrier. A player over an unloaded column fell more than five blocks in one second. `/tp` (`src/CommandSystem.cpp:208-218`) changes position without preparing destination terrain, making this relevant to teleportation and potentially movement that outruns streaming. The reproduction establishes absent-terrain falling, not its frequency during ordinary walking.

Fix: gate non-spectator movement on a loaded collision neighborhood, retain a safe position and prioritize destination generation. Avoid solving it with a large synchronous generation stall.

### 7. Far teleport distance calculations overflow

**Reproduced, P1.** `World::unloadDistant`, `src/World.cpp:2172-2178`, squares signed 32-bit chunk deltas. A teleport to X=1,048,576.5 blocks left origin chunks loaded in the executable check because a 65,536-chunk delta squared wrapped in this build. Signed overflow is undefined behavior, so other compiler results can differ. Commands allow coordinates up to 30 million blocks.

Fix: promote differences and products to 64-bit before arithmetic throughout distance checks. Test positive/negative far teleports, diagonal distances and world limits.

### 8. Far coordinates disable simulation-distance filtering

**Reproduced, P1.** `src/World.cpp:3791-3798` returns true for every queried position whenever either stream-center chunk coordinate exceeds 100,000 in magnitude. At X=1,600,016.5, the origin was reported simulation-active. An initialization sentinel has become a real-world coordinate condition.

Fix: track initialization explicitly; use 64-bit distance arithmetic for normal filtering at all supported coordinates.

## P2: gameplay correctness and frame pacing

### 9. Farming coordinate packing breaks negative Y and aliases distant X

**Reproduced for negative Y; inspected for X aliasing, P2.** `src/Farming.cpp:5-7,35-42` packs Y into 10 bits but never sign-extends it on decode. A crop planted at Y=-10 was looked up at 1014 and removed from the growth table; the positive-Y control advanced. X uses only 21 bits, so positions separated by 2,097,152 blocks share the X field despite the game's larger supported range.

Fix: use a full coordinate struct and hash or a packing scheme whose documented range includes all valid positions. Verify negative Y/X/Z and far positions.

### 10. Slabs and other partial blocks use full-cell targeting

**Reproduced for a slab, P2.** `World::raycast`, `src/World.cpp:4741-4776`, intersects actual geometry only for doors, torches and lightning rods. Other renderable blocks hit their whole voxel. A horizontal ray through the empty upper half of a lower Stone Slab hit the slab. This makes aiming, mining and placement inconsistent with visible geometry.

Fix: use the block's selection geometry consistently, including multiple boxes where necessary. Preserve deliberate selection boxes for plants.

### 11. Settings accept NaN for core camera/audio parameters

**Reproduced, P2.** `src/Settings.cpp:36-58,133-204`: `std::stof("nan")` succeeds and ordinary `std::clamp` does not sanitize NaN. FOV, sensitivity and master volume remained nonfinite after loading. Weather-specific float settings already check finiteness. Visual/audio consequences were not separately exercised.

Fix: validate all floating-point settings with finite defaults before clamping; test NaN, infinities, malformed and out-of-range values.

### 12. Binary player/mob validation is incomplete

**Reproduced for accepted nonfinite mob position; inspected for other fields, P2.** `src/Survival.cpp:2772-2797` accepts mob positions without finite/range validation and converts them into chunk coordinates. A saved NaN cow position loaded successfully. `src/Player.cpp:180-193` reads directly into live fields before the entire record is validated and uses clamps without finite checks. Bad data can reach coordinate conversions or leave partial player state on failure.

Fix: decode into temporary objects; validate every numeric field and supported range before committing. Keep invalid data out of simulation.

### 13. Autosaves serialize full world state on the main thread

**Inspected plus reported measurement, P2.** `Game::saveAll`, `World::saveWorld`, `Transportation::save`; autosave interval is 12 seconds (`src/Game.cpp:2036-2038`). Each save rewrites all stored edits and containers, and navigation saving waits for its outstanding map job. Existing v2.14.1 validation reports maximum navigation-data saves around 20 ms under stress. Cost grows with saved world history, even when most data is unchanged.

Fix: capture a coherent immutable snapshot, serialize it on a bounded worker and coordinate atomic commit/dirty revisions. Do not read mutable live state from a saving thread.

### 14. Teleport retirement can exceed streaming's soft frame budget

**Reported measurement and inspected, P2.** Release validation records approximately 37-63 ms teleport stalls. `src/WorldStreaming.cpp` budgets retirement between batches, but each allocator/driver operation is indivisible; `unloadDistant` also traverses the complete loaded set. The exact stall source remains unresolved in the release evidence.

Fix: profile allocator release, fence polling, GL deletion, page trimming, queue sorting and chunk detachment separately before changing architecture. Bound individual work where possible. Do not describe the cause as proven GPU synchronization.

### 15. Farming periodically performs a large synchronous search

**Measured microbenchmark and inspected, P2.** `src/Farming.cpp:19-30` scans up to 97 x 97 x 18 = 169,362 block positions every five simulation seconds, even with no crops. Twenty forced scans in the audit's RD2 fixture averaged 1.185 ms, worst 4.407 ms. This is a narrow microbenchmark with mostly unloaded outer columns, not a full-world frame-time prediction.

Fix: maintain per-chunk crop registration or an amortized scan cursor. Register loaded/generated/edited crops and bound main-thread work per frame.

### 16. Occlusion culling loses its proofs whenever the camera changes

**Inspected performance limitation, P2.** `src/TerrainOcclusion.cpp`, `TerrainOcclusion::draw`, requires exact view/projection equality plus unchanged world visibility state. Any change clears proofs and draws everything. Walking, turning and view bob can therefore remove its benefit. This is conservative correctness behavior, not evidence of holes or incorrect hiding.

Fix: measure culling during ordinary traversal first. Consider conservative temporal reprojection, hierarchical depth or another proof system that tolerates camera motion. Keep a safe visible fallback for disocclusion and edits.

### 17. Below 20 FPS, simulation intentionally loses elapsed time

**Inspected, P2.** `src/Game.cpp:926-930` caps elapsed time at 0.05 seconds and discards the rest. At a steady 10 FPS, only 0.5 simulation seconds advance per wall-clock second, affecting movement, world time and timers. Clipping also caps the displayed frame-millisecond metric at 50 ms, even though the separately sampled FPS can be lower.

Fix: retain raw time for reporting and use a fixed-step accumulator with bounded catch-up and an explicit overload policy. Validate behavior at 10/15/20 FPS and through frame spikes via the actual Game loop.

## P3: scaling and design limits

### 18. Current terrain LOD only removes small decorations

**Design limit, inspected, P3.** `include/TerrainLod.h` omits grass/ferns/dead bushes and, at level 2, some flowers/mushrooms. It does not simplify rock faces, cave geometry, structures, water or distant tree canopies. It deliberately avoids a horizon proxy to preserve caves. Calling this full terrain LOD would overstate it.

Improvement: measure geometric cost by surface type and investigate distant region meshes or silhouette-preserving simplification with edit/cave/water correctness checks. Current packed/indexed terrain and batching already provide substantial optimization.

### 19. Reload makes every saved animal persistent

**Inspected, P3.** `src/Survival.cpp:2791` assigns `persistent=true` to every loaded animal. The despawn rule at 2546-2564 preserves persistent animals. Natural mobs that would have despawned before a restart can survive after one, increasing long-session populations and contributing to finding 4. Actual accumulation rate requires a repeated-exploration benchmark.

Improvement: version and preserve persistence semantics, distinguish natural population from bred/structure/explicitly persistent entities, and test repeated save/reload/exploration cycles.

### 20. Entity processing and render preparation traverse global collections

**Inspected scaling risk, P3.** `SurvivalWorld::update` loops all animals/drops/orbs before simulation-distance checks. `renderCuboids`, `renderBillboards` and `renderItemSprites` build output across saved collections; renderer distance/frustum filtering happens later. Distant entities still cost traversal, temporary output and, for ordinary animals, multiple cuboids.

Improvement: spatially index entities, query active/visible neighborhoods before building render geometry, and consider instancing for cuboids. Benchmark with thousands of mostly distant entities before choosing complexity.

### 21. Furnaces are globally scanned; edit records retain reverted cells

**Inspected scaling risk, P3.** `src/World.cpp:3984-3994` checks every saved furnace every 0.2 simulation seconds. `setBlockInternal:3919-3920` retains an edit whenever a block changes, including changes back to generated terrain, with no baseline compaction in that path. These costs scale with explored/built history, not just currently active chunks.

Improvement: maintain active chunk/container lists and compact reverted edits against the correct historical generator, outside the frame-critical path. Preserve legitimate edits and generated-container semantics.

### 22. Maps have a permanent lifetime cap without reclamation

**Reproduced design limit, P3.** `src/Navigation.cpp:21-33`: map creation and zoom allocate identities until `maps_.size()` reaches 128. Old zoom levels stay stored and there is no deletion/reclamation API. After 128 creations, the next creation returned zero. Losing/discarding map items does not reclaim their records.

Improvement: communicate the cap; add safe map management or reclaim only identities proven unreferenced across inventories, containers and drops. Never recycle an identity still referenced by an item.

### 23. Save locking protects writes, not whole game sessions

**Inspected risk, P3.** `src/SaveFile.cpp`, `WriterLock`, holds a named mutex during write/recovery operations. It does not reserve a profile for a complete play session. Two instances can load the same older state and save sequentially without lock contention, allowing later stale-state replacement despite individually coherent checkpoints. This audit did not reproduce the two-process gameplay scenario.

Improvement: reserve a profile/world for one writable session or check a checkpoint generation token before committing. Make the second session read-only or explain the conflict.

### 24. Terrain memory targets only trim empty pages

**Inspected design limit, P3.** `include/World.h:252` provides nominal memory targets, but the target's enforcement in `World::releaseEmptyTerrainPages` only changes the idle-page reserve. It cannot shrink occupied pages below the terrain required by the selected render distance. The setting is not a hard VRAM limit; RD64 remains expensive.

Improvement: describe it as an allocation/retention target. Expose live and reserved usage; offer informed distance/quality guidance or an explicit adaptive policy rather than implying a hard budget guarantee.

### 25. Regression infrastructure misses these negative cases and normal traversal

**Inspected validation gap, P3.** Existing rendering benchmark phases are stationary/rotation/editing (`src/OptimizationBenchmark.cpp:98-169`) and inhibit player input during optimization benchmarking (`src/Game.cpp:1483-1484`). A separate streaming benchmark exists, so movement is not wholly untested, but it uses a spectator fixture and does not establish normal walking/boating with active survival, maps and weather. Short high-end tests do not establish low-end or multi-hour behavior. No tracked `.github` CI workflow was found. Existing smoke suites passed despite the reproduced persistence defects.

Improvement: add targeted regressions for findings 1-12; run them through production Game load/save paths. Add real survival traversal, boundary crossings, map-in-boat travel, disk failure and repeated reload cases; publish p95/p99/worst times and memory plateaus. Automate a fast subset on every change. Large Game/World/Survival/Renderer translation units also make isolation and review harder; extract services incrementally after correctness coverage exists.

## Suggested fix order

1. Sidecar load preservation, coherent death saves and health/hunger reload.
2. Writer/loader invariants, finite validation and per-world listing isolation.
3. Unloaded-terrain collision and coordinate arithmetic throughout the supported range.
4. Farming coordinate correctness, selection geometry and raw/fixed-step timing.
5. Save/retirement profiling, amortized farming and active entity/container indexing.
6. Movement-capable culling, broader distant LOD, map management and long-session coverage.

Do not start with another terrain-generator rewrite: this audit's highest-impact findings are persistence and simulation correctness.

## Reproduction and limits

The audit driver is in `build/audit-20261016/main.cpp`; it links production Release objects from `build/installed-release`. Its CMake file adds embedded resources and aliases for the private Game entry points used by the harness. It does not modify production functions. Build with `cmake --build build/audit-20261016/out --config Release`.

Run `AuditDriver.exe` from the isolated `build/audit-20261016/profile` working directory for subsystem checks. Run it with `--game` from `build/audit-20261016/game-profile` for real Game paths. These fixture directories are disposable and intentionally contain invalid save data. Do not run the driver from a real user profile: unlike production main, the custom driver does not invoke UserData initialization or honor its environment override. One initial game-path run used the repository directory; its newly created test/import directories were subsequently moved under the audit fixture. The repeated checks and retained logs use the isolated directories. Existing root world/inventory/player/mob files were not targeted by the test's world saves; global settings were re-saved by that initial run.

Retained evidence:

- `build/audit-20261016/profile/isolated-checks.log`: subsystem results and farming timing.
- `build/audit-20261016/game-profile/game-checks.log`: real Game relog, death-save and invalid-inventory results.
- `build/audit-20261016/regression-profile/logs/VoxelFrontier.log`: earlier transportation smoke regression, ending in a pass.
- `docs/TRANSPORTATION_POLISH_VALIDATION.md` and `docs/RELEASE_v2.14.1.md`: existing save/teleport measurements and disclosed hardware/coverage limitations.

No new full performance matrix, low-end hardware test, multi-hour soak or installer upgrade test was performed. Microbenchmark times are specific to this run and fixture. Reproduction harness files/logs are under the ignored build tree and are not automatically part of a Git commit. Production game source remains unchanged.
