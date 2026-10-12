# v2.12.1 gameplay validation

Status: Release validation completed for v2.12.1. Publication authorized on 2026-10-12.

## Implementation and root causes

- **Structures:** Generator V20 keeps V19's placement/eligibility and terrain foundation, with larger chambers and more rooms/loops. Storage/workbench/library alcoves stay outside connector axes. A collapsed endpoint is checked against the entire nearby passage volume so it cannot seal a connection at another height. Separate V19/V20 bounded planning caches preserve legacy generation even after loading an older save into a newer World instance.
- **Fluids:** the previous solver only recognized water; lava had no flow states. Inactive updates were discarded. `WorldFluids.cpp` now shares bounded propagation/retraction between water and lava, defers inactive jobs, schedules loaded boundaries, and delays lava to one in four 0.1-second ticks. Five new lava states are appended after existing IDs. Sources/edits remain in the existing save format; derived flowing cells are reconstructed.
- **Hazards:** a single `Burning` timer refreshes instead of stacking, accumulates whole-second damage and clears on water contact. Double timer accumulation avoids losing the fifth tick at 60/144 FPS. Player and mob checks use actual intersection bounds and fluid heights. Existing batched particles handle fire/smoke. Creative/Spectator are immune. Burning timers are transient like other combat cooldowns; health still uses existing persistence.
- **Cobweb:** intersection applies a 0.25 movement multiplier; adjacency does not. Normal movement rules return after exit.
- **Permissions:** `CHEATS1` metadata is per world. New worlds default Off; missing legacy metadata preserves the previous On access. The command executor requires both installed-build capability and world permission, independently of UI. Demo commands cannot bypass this by supplying an enabled command context. Pause-menu mode selection is removed.
- **Spawning:** the old daytime rejection prevented cave spawning. Bounded candidate probing now checks effective sky/block light <=6, actual floor/headroom, biome exclusions and existing distance/cap rules. Finite structure encounters remain separate.
- **Inventory:** the constructor no longer grants starter items. Both Creative and Survival dropping remove the actual selected stack quantity; the Creative catalogue remains unlimited.
- **Icon:** Cracked Stone Bricks uses the same native cube projection/shading as solid block icons. Only its atlas cell changes: native pixels x64..79/y224..239; expanded x256..319/y896..959.

## Release correctness results

Both full and standalone executables were copied to `%TEMP%/VoxelFrontier-v2121-review` and run outside source/build directories with isolated profiles.

Passed in both builds: command, survival, weather, patch272, menu and fluid smoke fixtures. These cover parser/permissions, demo denial, old items/block states, mesh edits/AO, movement/combat, weather assets/settings, inventory/container persistence, menu resizing/world reload and streaming invariants.

Specific new results:

- Water seven-block and lava four-block horizontal limits across a chunk boundary; downward flow, source removal/retraction, opened-block updates, water geometry and installed source save/reload.
- Burn: exactly five HP over five seconds at 30/60/144 FPS and 0.75-second timer steps; refresh/non-stacking, water clearing, actual player/mob damage and Spectator immunity.
- Cobweb: actual-volume intersection, no adjacent-cell slowdown; walking 1.55 blocks/s inside versus normal 6.2 after exit.
- Controlled habitat fixture: daytime surface 0 hostiles, nighttime surface 17, daytime dark cave 17, torch-lit cave 0. Existing equal-role selection covers all four billboard types.
- Fresh inventory empty; single-item and whole-stack removal correct. Per-world Cheats default, On/Off metadata persistence, and legacy permission fallback tested.
- Underground audit: 43 mineshafts, 71 dungeons, 17 ruins; **131 connected layouts**. Deterministic loot, finite encounter persistence, modified/destroyed chests across unload/reload and saves passed.
- **2,352 historical chunk hashes match generators 1–19** across the stored fixtures. Generator V20 save/reload and append-only content IDs passed.

Measured plan cell growth across three seeds: mineshafts **36.5%**, dungeons **22.8%**, ruins **27.8%**. Dungeon chambers grow in all dimensions; the fixed cave-connection contribution makes total cell growth slightly below the requested 25% lower guide. Navigation is retained instead of expanding corridors to chase this statistic.

## Evidence

Local artifacts under `build/v2121-validation/`:

- `audit/structures.csv`, `historical.csv`, `generation.csv`, `structure-generation.csv`, layout and loot CSVs.
- `smoke/full-*` and `smoke/demo-*` isolated logs/captures.
- `captures/`: real renderer captures of mineshaft rooms/corridors, dungeon, ruins/stairs, fluids/hazards and Creative inventory.
- `performance/v19-rd*/streaming.csv` and corresponding `v20-rd*` files.

## Limits

These are bounded deterministic fixtures, not proof across every possible seed or long-running multiplayer scenario. No integrated-GPU hardware was available. Larger structures can add local geometry even though unchanged terrain regions remain comparable. Lava/water mixing uses the existing fluid/block interaction scope; no new obsidian/cobblestone reaction or infinite-source mechanic is introduced. Existing fluid textures and renderer pipeline are retained.

## Performance comparison

Hardware: Intel Core i7-14700F / AMD Radeon RX 9060 XT. Release embedded full builds, seed 123456789, 1280x720, simulation distance 8, weather graphics Off, MSAA Off, uncapped FPS. Sequential existing streaming fixture: 20s stationary warmup, 8s 8x Spectator flight, 12s recovery, 20s distant teleport. V19 baseline is the v2.12.0 executable; V20 is the new build. These runs exercise streaming/rendering, not player physics simulation.

Before -> after; RAM is process working set MiB.

| RD | Phase | FPS | Mean ms | p99 ms | RAM MiB |
|---|---|---:|---:|---:|---:|
| 8 | warmup | 2676 -> 2663 | 0.374 -> 0.375 | 0.956 -> 1.039 | 241.9 -> 243.3 |
| 8 | flight8x | 3395 -> 3353 | 0.295 -> 0.298 | 1.297 -> 1.366 | 264.1 -> 248.0 |
| 8 | recovery | 4094 -> 4011 | 0.244 -> 0.249 | 0.622 -> 0.650 | 257.2 -> 247.5 |
| 8 | teleport | 1978 -> 1974 | 0.506 -> 0.506 | 1.023 -> 1.014 | 269.7 -> 256.6 |
| 16 | warmup | 946 -> 965 | 1.057 -> 1.037 | 2.462 -> 2.201 | 310.2 -> 313.1 |
| 16 | flight8x | 973 -> 972 | 1.028 -> 1.029 | 4.464 -> 4.309 | 310.4 -> 307.5 |
| 16 | recovery | 1280 -> 1272 | 0.781 -> 0.786 | 1.176 -> 1.159 | 313.9 -> 315.2 |
| 16 | teleport | 862 -> 903 | 1.160 -> 1.108 | 2.280 -> 1.677 | 335.3 -> 328.7 |
| 32 | warmup | 352 -> 354 | 2.837 -> 2.824 | 6.456 -> 6.442 | 534.8 -> 535.0 |
| 32 | flight8x | 327 -> 327 | 3.059 -> 3.057 | 6.265 -> 6.410 | 570.6 -> 572.2 |
| 32 | recovery | 368 -> 369 | 2.717 -> 2.708 | 3.104 -> 3.110 | 571.3 -> 574.3 |
| 32 | teleport | 391 -> 391 | 2.559 -> 2.561 | 4.327 -> 4.372 | 656.5 -> 658.1 |

Most mean frame times differ by approximately 0–2% (RD16 teleport was 4.5% faster); a single run per configuration does not establish a meaningful improvement/regression. RD32 teleport RAM is 656.5 -> 658.1 MiB. Worst frames and stage timing/geometry are retained in CSVs; averages are not a guarantee of spike-free gameplay.

Structure-containing chunk generation, three repeats (same selected chunk coordinates; milliseconds for the entire batch):

| Structure | Chunks | V19 ms | V20 ms |
|---|---:|---:|---:|
| Mineshaft | 32 | 148.21 | 147.76 |
| Dungeon | 10 | 48.23 | 48.72 |
| Ancient Ruins | 24 | 73.10 | 73.42 |

The batch differences are small and do not support an optimization claim. Raw generation-byte and hash data remain in the audit CSVs. No low-end device was physically tested.

RD8 simulation comparison using the existing weather fixture (four seconds per quality after settling, same seed/settings, non-spectator gameplay simulation). Both runs deliberately end after the four RD8 samples; RD16/32 were already measured by the streaming fixture.

| Weather quality | Baseline simulation ms | New simulation ms |
|---|---:|---:|
| Off | 0.0120 | 0.0153 |
| Low | 0.0124 | 0.0138 |
| Medium | 0.0268 | 0.0248 |
| High | 0.0317 | 0.0308 |

Simulation remains under 0.04 ms in this quiet fixture. It is not a dense lava-edit or crowded cave stress benchmark; small differences are not conclusive.

Final packaging verification: installed full executable, standalone demo and installer all report ProductVersion 2.12.1. SHA-256 hashes match `dist/SHA256SUMS.txt`. The complete full/demo smoke suite was rerun after the final fixture edits and passed. Installed reset smoke also passed with an isolated sandbox profile. `git diff --check` is clean; HEAD remains the v2.12.0 checkpoint.

Visual inspection included the real Creative inventory cube icon, expanded mineshaft/dungeon/ruins rooms, stair corridor, and `captures/hazards-final/screenshots/voxel_1791802113462.bmp` showing the finite water/lava test channels and a live mob hazard enclosure. These are deterministic fixture captures, not a survey of every generated structure.

## v2.12.1 follow-up: GUI drops, fluid reliability and Generator V21

The earlier tables in this report describe the first v2.12.1 build. The follow-up remains uncommitted/unpublished and keeps the same application version, with **Generator V21** for new worlds. Generators 1–20 retain their exact geography and structure plans.

### Root causes and fixes

- GUI input never dispatched Q drops; the gameplay handler runs only with the GUI closed. Outside-click dropping already existed, but releasing a drag outside had no handler. GUI now owns Q/Ctrl+Q and outside drag release, with an early return after one drop operation. `Inventory::takeGuiDrop` consumes a held cursor stack or a hovered actual player slot; a bare catalogue entry is never treated as owned inventory. Click the unlimited catalogue to obtain a cursor copy first. Single and whole-stack removal use the same canonical storage as the hotbar.
- Lava deferred jobs using `fluidTick % 4`. A bounded queue can revisit a job repeatedly at a non-lava phase, starving it despite valid notifications. Per-job due ticks now age independently of the traversal phase. The deadline map contains deferred queued jobs only; it is cleared on world rebuild/load and erased when jobs are resolved, invalidated or unloaded. Water retains 0.1-second updates and lava retains approximately 0.4-second updates.
- A source job previously returned without activating downstream cells. A consumed/missing initial or chunk-boundary notification could therefore leave empty neighbors inert. Processing a source now queues only eligible dependent cells; settled sources do not create perpetual neighbor work. Jobs still read current blocks, coalesce by coordinate, and use bounded update budgets. No whole-world scan is added.
- V21 increases eligible underground candidate regions from 34% to 42.5%, retaining the 12:18:4 type weighting, region spacing, cave eligibility, depth gates and surface reservations. Independent deterministic decoration hashes add high off-axis corner cobwebs without shifting layout RNG, loot or encounters. Existing V19/V20 plans use separate unchanged caches.

### Measured structure frequency

Three identical seeds (123456789, 20260917, 42), each a 16,384×16,384-block area, comparing accepted plans after all cave/reservation eligibility gates:

| Type | V20 | V21 | Change |
|---|---:|---:|---:|
| Mineshaft | 234 | 288 | +23.1% |
| Dungeon | 324 | 406 | +25.3% |
| Ancient Ruins | 62 | 79 | +27.4% |
| Total | 620 | 773 | +24.7% |

Smaller 8,192-block samples have more rarity noise (131 -> 168 total). The larger counts, rather than a single seed or raw candidate probability, establish the frequency target. Region reservations and dimensions are retained; frequency does not increase structure size.

### Follow-up regression results

- Repeated water/lava placement/removal, source-only activation, a one-cell tick budget, blocked channel retraction/reopening, exact 7/4-block ranges, downward flow, boundaries and source save/reload passed. Burning, extinguishing and cobweb movement regressions remain passing.
- Creative actual-slot single/full-stack GUI drops, cursor single/full-stack drops, catalogue distinction, exactly one dropped stack operation, pickup back into the hotbar, and inventory save/reload passed. Survival shares the same removal primitives.
- Navigation/cave/room/chest-access validation passed for 168 accepted layouts. **Every accepted V21 structure contains cobwebs.** Modified/destroyed chest persistence and finite encounters passed.
- **2,928 historical hashes match generators 1–20**, including the prior V20 baseline. Versioned world saves load through V21.
- Follow-up artifacts: `build/v2121-followup/audit/frequency-16384.csv`, `frequency.csv`, `historical.csv`, structure/layout/loot CSVs; full/demo regression profiles under `build/v2121-followup/smoke/`.

Performance safeguards: the fluid budget remains bounded (96 jobs per tick in ordinary gameplay); overdue jobs progress without global scans. Sparse ceiling decorations add negligible planned geometry. Structure frequency naturally adds local generation/geometry in newly accepted regions. No low-end machine or extended worst-case fluid stress run was available; the original RD8/16/32 results above are not claimed as new follow-up benchmarks.

Final follow-up build check: both full and demo Release variants rebuilt successfully. Command/survival/weather/patch272/menu/fluid suites passed in `%TEMP%/VoxelFrontier-v2121-followup-review` outside source/build directories, with isolated profiles. Distribution hashes match `dist/SHA256SUMS.txt`; `git diff --check` is clean. No commit, push, tag or release was created.
