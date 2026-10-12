# v2.12.0 — The Exploration Update

Validated October 12, 2026. Approved for the **v2.12.0** release. New worlds use
Generator 19; saved worlds retain their stored generator.

## Implementation

`UndergroundStructures` builds immutable, world-coordinate plans for 256-block
regions. Seeded placement, depth/biome eligibility, region margins and existing
surface-structure reservations prevent neighboring plans from overlapping.
Each world caches at most 128 region results, including rejected candidates.
Planning and chunk-clipped placement use the existing generation workers.
OpenGL, meshing, streaming priorities and save serialization stay in their
existing systems; there is no per-block structure search.

- Mineshafts have 4–7 rooms, branching corridors, optional loops, wooden platforms
  and supports, cobwebs, stair connections, storage and collapsed dead ends.
- Dungeons have 1–3 rooms of varying size, cobblestone/mossy/cracked materials,
  treasure and finite Pillager encounters. Shallow dungeons have at most one
  generated enemy; deeper layouts have at most three.
- Ancient ruins have 3–6 rooms, corridors/stairs, collapsed corners and a secluded
  library treasure alcove. Deep ruins have an extra rarity gate and better loot.
- Every accepted plan has a dry natural cave entrance selected using the exact
  existing density lattice, slope mask and aquifer rules. Full-block stairs and
  platforms preserve a navigable route; room corners have foundations extending
  down to solid terrain where they intersect caverns.
- Loot uses reusable weighted Common/Uncommon/Rare tables. Chests receive 3–5
  rolls, or 4–6 in ruins, with at most one rare roll. Depth increases uncommon and
  rare chances. Diamonds are limited to 1–2 per rare diamond roll.
- Surveyor Token, Cavern Shard and Ancient Seal are original collectible items
  with native 16×16 icons. Cobweb and Cracked Stone Bricks are appended block IDs.
  Webs are decorative crossed geometry, not a new slowing mechanic.
- Existing 27-slot chest storage retains generated, emptied, modified and broken
  chests. Existing encounter-marker persistence prevents duplicate encounters.
  The integration now accepts valid negative-Y mob markers instead of rejecting
  everything below Y2. Existing simulation-distance gating remains active.

Generator 19 adds structures after the existing V18 generation stages. V18
geography, climate, density, vegetation, caves, rivers, aquifers, ores, sea level,
natural Y256 ceiling and world bounds remain. World save schema is still version
2; old block/item IDs, old loot tables and old generator branches are unchanged.

Design inspiration: [Minecraft's mineshaft overview](https://www.minecraft.net/nl-nl/article/mineshaft).
These regional layouts and pixel assets are original to Voxel Frontier.

## Distribution and accessibility

Seeds **123456789, 20260917, 42**, each sampled over an 8,192×8,192-block region:

| Type | Structures | Rooms | Chests | Generated enemies | Floor range |
|---|---:|---:|---:|---:|---|
| Mineshaft | 43 | 235 | 142 | 43 | −15..55 |
| Dungeon | 71 | 123 | 113 | 100 | −44..44 |
| Ancient ruins | 17 | 64 | 43 | 34 | −44..15 |

All **131 layouts** passed a player-sized grid traversal: every room center,
chest access tile and cave entrance is reachable with supported footing,
two-block body clearance and at most a one-block step. Encounter positions have
solid floors and clear bodies. Layouts stay inside their region and avoid existing
surface-structure footprints. The first three layouts of each type also passed
independent chunk-clipped reassembly, repeat generation and four simultaneous
generation calls. These include negative coordinates, negative Y and chunk borders.

![Distribution, seed 123456789](../build/v212-validation/audit/123456789-structure-distribution.png)

Equivalent maps for seeds [20260917](../build/v212-validation/audit/20260917-structure-distribution.png)
and [42](../build/v212-validation/audit/42-structure-distribution.png) are available.
Raw [structure records](../build/v212-validation/audit/structures.csv) include type,
depth, biome, room/chest/enemy counts and entrance coordinates.

## Loot, saves and regressions

- **90,000 synthetic chests** across three depths and three tables: deterministic
  items/counts/durability, valid stack limits and increasingly valuable deep loot.
  [Loot frequencies](../build/v212-validation/audit/loot.csv) report roll counts,
  diamond quantities and relic rolls separately.
- A generated chest was emptied and modified, then regenerated, actually unloaded
  and reloaded, saved and restored. It retained empty slots and a relic in slot 26.
  Destroying it, saving and reloading did not regenerate either the block or loot.
- Negative-Y encounter markers passed actual chunk integration, paused activation
  gating and queue deduplication. Saved finite encounters did not respawn on reload.
- **1,776 historical chunk hashes** for generators 1–18 matched the validated
  fixtures. The 1–10 fixtures intentionally reuse their original warmed probe/cache
  protocol. Saved generator versions 1–19 survived load and produced identical
  unexplored test chunks after reload.
- Old world texture pixels and all 143 historical inventory atlas cells compare
  exactly with the v2.11.3 Git checkpoint. All five new icons are native 16×16.
- Full and demo Release variants passed command/content, Survival/combat,
  physics/respawn, lighting/AO, fluids, async mesh/streaming, weather and menu checks
  from executable copies under `%TEMP%/VoxelFrontier-v212-review`, outside the
  source/build directories. Installed menu tests verified three independent worlds;
  demo checks verified persistence bypass and fresh sessions.
- Full and demo underground audits passed. Save tests apply to the full build;
  they are explicitly skipped in the stateless demo.

## Actual engine views

Inspected seven rendered views of room interiors, wooden supports/cobwebs,
mineshaft corridors, a dungeon below Y0, ruin corridors/stairs and a treasure
alcove. Captures use the shipped OpenGL renderer and generated torches, without
extra preview torches. Seed 123456789: mineshaft near (3220,34,−3945), dungeon
near (3225,−39,−3733), ruins near (3211,15,−2962).

![Actual engine views](../build/v212-validation/captures/exploration-views.png)

![Original exploration icons](../build/v212-validation/exploration-icons.png)

## Performance

Hardware: **i7-14700F (28 logical processors), Radeon RX 9060 XT**. Baseline:
preserved v2.11.3 Release binary, generator 18. New binary: Release, generator 19.
Same seed 123456789, 1280×720, simulation distance 8, default daytime, clear
weather, F3/VSync/MSAA off, uncapped existing streaming fixture. Each RD run has
20-second warmup, 8-second 8× flight, 12-second recovery and 20-second teleport
recovery. This fixture is a controlled rendering/streaming workload, not a normal
gameplay FPS promise. There is one paired run per RD; tiny differences are noise.

Settled recovery phase:

| RD | FPS V18 → V19 | Mean ms | p99 ms | Render ms | Process RAM MiB |
|---|---|---|---|---|---|
| 8 | 4334.8 → 4326.1 | 0.231 → 0.231 | 0.480 → 0.472 | 0.215 → 0.215 | 265.2 → 243.7 |
| 16 | 1318.7 → 1313.1 | 0.758 → 0.762 | 0.910 → 0.912 | 0.742 → 0.744 | 305.5 → 315.1 |
| 32 | 370.4 → 370.8 | 2.700 → 2.697 | 3.010 → 2.881 | 2.640 → 2.663 | 569.5 → 575.4 |

8× flight:

| RD | Mean ms V18 → V19 | p99 ms V18 → V19 | Highest single frame across all phases | Highest sampled RAM MiB |
|---|---|---|---|---|
| 8 | 0.274 → 0.274 | 0.702 → 0.724 | 21.79 → 20.45 | 277.2 → 255.4 |
| 16 | 1.005 → 0.991 | 4.241 → 4.125 | 22.50 → 23.63 | 322.9 → 326.1 |
| 32 | 3.056 → 2.989 | 6.465 → 6.130 | 23.73 → 26.45 | 654.3 → 658.0 |

There is **no meaningful measured FPS change**. Isolated worst frames vary;
RD32's largest V19 frame is 2.72 ms longer. Loaded chunks were identical after
recovery (246 / 891 / 3400). At RD32, terrain diagnostics reported 41,791,758 →
41,791,788 vertices and 2,006,004,384 → 2,006,005,824 used terrain-buffer bytes.
The substantial existing high-RD geometry cost is preserved, not hidden by a
quality or distance reduction. Recovery cleared outstanding work in about
0.17–0.25 seconds after flight in these runs.

Generation over 576 matched chunk builds per version, three seeds and three
repeats: **3.258 ms → 3.239 ms mean**, a negligible difference. Chunk block bytes
were unchanged at the origin workload. Separate accepted-structure workloads
cover entire mineshaft/dungeon/ruin footprints: first-plan cost is approximately
**1–6 ms extra per regional plan**, on the generation worker. Cached repeat
footprint costs are close to V18. This is additional content cost, not an
optimization claim. See [generation](../build/v212-validation/audit/generation.csv),
[structure footprint timings](../build/v212-validation/audit/structure-generation.csv)
and [all streaming measurements](../build/v212-validation/performance/summary.csv).

## Reproduction and limits

Build/package both variants with `./package_release.ps1 -BuildJobs 8`. Run
`tools/run_underground_validation.ps1 -HistoricalBaseline <baseline.csv>` using
the historical `seed,version,x,z,hash` fixture. Then run
`python tools/build_exploration_previews.py <audit-directory>` (NumPy/Pillow).
Always use isolated user data for fixtures. Existing commands include
`--command-smoke`, `--survival-smoke`, `--weather-smoke`, `--patch272-smoke`,
`--menu-smoke` and `--streaming-benchmark=<directory>`.

The finite sample does not prove every possible seed/intersection. A connected
natural cave is not guaranteed to lead directly to daylight. Collapses are
deliberate dead ends/corner rubble; hidden treasure is a screened alcove, not a
new puzzle/locked-door system. There are no continuous spawners, new enemy types,
bosses, armor or relic powers. Direct integrated-GPU/dual-core tests and lengthy
travel/soak tests were not performed; no low-end hardware results are claimed.
RD64 was not required for this focused content update and was not benchmarked.

Build outputs are in `dist/`, with SHA256 checksums.
