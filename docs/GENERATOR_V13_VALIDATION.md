# Generator V13 — Terrain Aesthetics validation

Validated source update for **v2.11.2-beta2**, tested on 2026-10-11. New worlds use
Generator V13; loaded worlds retain their saved generator. The earlier v2.11.2-beta
artifacts contain Generator V12. Benchmark binary hashes below identify the original
validation builds; release binaries are rebuilt with beta2 metadata and have separate checksums.

## Findings and implementation

The V11/V12 surface equation adds a positive 3D noise term to a height-derived density.
Its envelope is zero at the nominal surface, then rises again above that surface. This
can create a second solid band separated from the land below: stone pancakes and blobs.
Surface material and vegetation selection use the nominal height, so those added lobes
also tend to be bare rock. The elevation-based biome selector additionally makes many
warm, wet hills bare Mountains/Stony Peaks.

V13 changes those causes in focused `TerrainAesthetics` / `WorldAesthetics` modules:

- A continuous height envelope combines continental elevation, broad mountain belts,
  rounded/rugged ridge profiles, erosion, rolling foothills and regional dry plateaus.
  River valleys, marshes, islands and the inherited coastal datum remain.
- The existing cached 3D lattice now subtracts slope-masked cliff recesses beneath a
  protected surface roof. It does not add a disconnected positive density band above
  the envelope. Roof depth uses world-coordinate neighbor heights at one/four-block
  offsets, joining caps into adjacent higher terrain without a chunk-local flood fill.
- Cheese, spaghetti, noodle and pillar fields retain their underground curves. The
  protective roof changes their near-surface influence; ravines and connected entrance
  planning remain. Aquifer rules, ore distributions and the density lattice are reused.
- Medium habitat/humidity variation selects compatible biome families. Macroclimate and
  continental samples remain identical to V12. Warm/wet foothills retain woodland below
  the snowline, with dry woodland buffering hot desert-to-forest transitions.
- Steep slopes expose stone, gentler slopes retain soil/grass. Trees use deterministic
  support checks, thick trunks check all four root columns, and canopy heights vary.
  Sparse spruce is permitted on suitable lower Stony Peaks. V12 mushroom spacing stays.
- Generation scans less empty space above V13 surfaces. The existing asynchronous worker
  pools, streaming priorities, cached columns, greedy meshing and GPU pipeline remain.
- Entrance-plan caches include generator version as well as seed. Switching worlds or
  paired audit versions cannot reuse a plan from another terrain algorithm.

No block/item IDs, save-record layouts, renderer/shaders, physics, mob behavior or weather
mechanics changed. Y=-64..319 and sea level 64 remain. The loader accepts generator 13 in
the existing metadata field; algorithms 1–12 are still selected for existing worlds.

## Maps and real rendered evidence

Seeds: **20260917, 123456789, 42**. Paired 512×512-sample maps cover **4,096×4,096** and
**30,720×30,720** blocks, with labeled biome, temperature, humidity, continentalness and
height panels. These are sampled overviews, not one-pixel-per-block maps.

Files are development outputs under `build/v13-final-validation/audit/`. For example:

- `20260917-4096-comparison.png`
- `123456789-30720-comparison.png`
- `42-30720-comparison.png`
- Paired `terrain-<seed>-v<version>-<scene>.bin` volume exports and V13 cutaway images

The generated maps were visually inspected, followed by actual in-engine screenshots
using identical seed/camera/time/settings for each pair. Twenty ground/flight views are
under `build/v13-final-captures-validated/`; additional coast, valley, cave-mouth and
undercut views are under `build/v13-detail-captures/`. The screenshots are ignored test
artifacts, not runtime dependencies or committed documentation assets.

Reference comparison observations:

- Forest/jungle: connected tree-covered hills, varied canopies and grassy slope breaks
  replace the original bare/floating formations at the tested habitat sites.
- Rocky mountains: connected slopes, ridges and a visible ravine remain, with attached
  recessed cliff roofs rather than a debris field. A close view confirms an undercut
  beneath connected rock, not just a height-map silhouette.
- Savanna: acacia-covered rolling uplands, broad ridge masses and a steep connected valley.
- Taiga: forested foothills lead into an exposed snowy summit; tree roots remain grounded.
- Coast and cave-mouth captures confirm ordinary shore terrain and discoverable openings.

These are improvements in landform geometry, not a claim to reproduce the reference
renderer, shaders, vegetation artwork or atmospheric effects. Voxel terraces remain
visible, and some mountain profiles are still broad rather than jagged.

### Biome/route measurements

| Seed | V12 regions, 30,720 overview | V13 regions | V12/V13 ocean components |
|---|---:|---:|---:|
| 20260917 | 8,337 | 12,966 | 6 / 6 |
| 123456789 | 8,338 | 13,167 | 6 / 6 |
| 42 | 7,153 | 10,560 | 7 / 7 |

All **35 biomes** were found for each seed across the two overview extents. Macro
temperature, macro humidity, continentalness and river fields match **exactly at
1,572,864 paired map samples**. Matching ocean
component counts support preserved large-scale connectivity; they do not prove every
coastline pixel is identical. Temperature/humidity microvariation and altitude cooling
still affect local biome selection.

There are 64 deterministic routes per seed, sampled every eight blocks at three lengths.
For routes with more than 80% land:

| Route length | V12 mean biome types | V13 mean biome types | V12 / V13 qualifying routes |
|---|---:|---:|---:|
| 500 blocks | 2.59 | 2.89 | 86 / 85 |
| 1,000 blocks | 3.71 | 4.35 | 78 / 78 |
| 1,500 blocks | 4.82 | 5.26 | 73 / 72 |

Individual routes can still stay in one biome. These are sampled exploration statistics,
not a promise of a biome change every fixed distance.

### Connectivity and geometry

Thirty paired **96×96×384** terrain volumes include both selected representative habitats
and retained fixed V12 problem coordinates. Six-neighbor components that touch a lateral
volume edge have unknown outside support and are reported separately, not called floating.
Roots are components connected to the bottom terrain layer. Vegetation, fluids and ice
features are excluded from the rock-component test.

| Metric, all sampled volumes | V12 | V13 |
|---|---:|---:|
| Detached near-surface rock components | 49 | 1 |
| Detached near-surface rock blocks | 21,611 | 1 |
| All enclosed detached rock blocks, including deep caves | 22,743 | 1,134 |
| Exposed rock faces, including caves | 3,268,712 | 3,241,874 |
| Generated block storage bytes | 69,709,824 | 71,553,024 |

The near-surface debris volume falls by **99.995%** in these samples. The remaining surface
component is a single block; this is not a claim that every world contains zero isolated
voxels. Tiny deep cave chips remain, preserving underground carving rather than applying
an indiscriminate terrain-deletion pass. Exposed rock faces decrease **0.82%**, while sampled
block storage rises **2.64%** because the connected land mass differs.

`undercut_columns` in the CSV includes cave roofs. It is a broad roof/cavity measure, not
an exact count of visible mountain arches. A separate offline exterior-roof diagnostic
requires a four-block air gap, three-block rock roof, depth within 40 blocks of the local
top, height above sea level, and an unobstructed cardinal corridor to a lower exposed
column within 12 blocks. This finds **5,834 / 150,154 land columns (3.89%)** in V12 and
**538 / 142,563 (0.38%)** in V13. It includes shallow exterior caves and does not by itself
prove that a roof is attached. Combined with component tests and real undercut captures,
it supports rare exterior recesses rather than pervasive floating roofs. A global
overhang-frequency or arch-count guarantee is not established.

## Correctness and compatibility

- Generators 1–10: **480 distinct chunk hashes** match the stored pre-change baseline.
- Generator 11: **432 repeat rows / 144 distinct chunks** match its baseline.
- Generator 12: **432 repeat rows / 144 distinct chunks** match a fresh audit of the
  preserved pre-change executable. An older stored V12 audit differed; it was rejected
  as the release reference and the actual executable was rerun before accepting results.
- Total checked legacy coverage: **768 distinct chunks / 1,344 hash rows**, zero mismatches
  against the confirmed baselines. This is representative coverage, not every world chunk.
- V13: repeated and parallel generation, shared density-lattice seams, negative coordinates,
  world bounds, upper/negative-Y edits, meshes, lighting and versioned save roundtrips pass.
- Worker tests: eight byte-identical parallel meshes, worker plans, teleportation,
  revision/epoch cancellation, bounded queues and exact GPU upload pass.
- Structures on real V13 terrain: 651 / 753 / 696 validated regions for the three seeds,
  including village/outpost/pyramid biome, slope and loot checks.
- Connected dry entrance passages: 5 / 1 / 4 plans in the sampled approximately 1.5 km areas;
  generated two-block-clear passages and native cavity endpoints are checked.
- Full Release fixtures: menu/multiple-world isolation, weather, commands/content,
  survival/save compatibility, respawn/tooltips and spectator transitions pass.
- Safe spawn/respawn fixtures were repeated on all three seeds, including distant death,
  unloaded spawn terrain, retained downward velocity and Creative-to-Survival death.
- Demo Release fixtures: menu, weather and embedded V13 terrain preview pass; no persistent
  world/settings/player/inventory/mob files or save directories are written.

## Performance methodology

Hardware: **Intel i7-14700F, 28 logical processors; AMD Radeon RX 9060 XT**. These results
do not establish integrated-GPU performance. Both full and demo Release configurations
build successfully with the existing embedded assets and GUI entry point.

Single-thread chunk generation, 432 timed rows per version in the final audit:

| Metric | V12 | V13 | Change |
|---|---:|---:|---:|
| Mean generation ms | 2.968 | 3.089 | +4.06% |
| Median generation ms | 2.685 | 2.773 | +3.27% |

Connected mountain/woodland geometry changes storage and meshing workloads, so this is
not a behavior-identical optimization benchmark. No compiler or other game fixture runs
concurrently with the final serial frame-time matrix.

The serial matrix uses the same current Release executable with generator 12 then 13,
seed 123456789, 1280×720, simulation distance 8, fixed time 35 seconds, clear weather,
weather graphics Off, F3 Off, VSync Off, no MSAA and uncapped frames. Each RD runs 20 s
warmup, 20 s of 8× spectator flight (68 blocks/s forward and 17 sideways), 40 s recovery,
then 20 s after a teleport. Each version therefore runs for approximately 400 seconds.
Camera/path and settings are identical; changed terrain legitimately changes mesh work.
This fixture measures streaming plus rendering, not full mob/particle-heavy gameplay.

### Flight frame times

| RD | FPS V12 → V13 | Mean ms V12 → V13 | p99 ms V12 → V13 | Worst ms V12 → V13 |
|---|---:|---:|---:|---:|
| 8 | 4,803 → 4,737 | 0.208 → 0.211 | 1.677 → 1.694 | 17.680 → 15.604 |
| 16 | 1,000 → 1,003 | 1.000 → 0.997 | 4.070 → 4.138 | 17.569 → 16.886 |
| 32 | 289.0 → 285.7 | 3.460 → 3.500 | 6.845 → 6.838 | 18.742 → 17.413 |
| 64 | 127.7 → 124.1 | 7.832 → 8.055 | 19.507 → 18.339 | 24.786 → 28.876 |

Mean flight changes are +1.39%, -0.32%, +1.16%, +2.86%. These small differences are one
serial comparison, not confidence intervals or proof of an optimization gain. After
settling, RD64 mean is **8.953 → 8.931 ms**, p99 **10.790 → 10.718 ms**. Recovery clears all
generation, dirty, mesh, upload, retirement and lighting queues in **12.23 → 11.65 s**.
The teleport phase still has large individual spikes: **131.22 → 104.22 ms** at RD64.
That remains an engine limitation; the smaller single observed maximum is not presented
as a guaranteed frame-pacing improvement.

### Flight memory and geometry at phase end

| RD | Working set MiB V12 → V13 | Terrain vertices millions V12 → V13 | Used buffer MiB V12 → V13 | Capacity MiB V12 → V13 |
|---|---:|---:|---:|---:|
| 8 | 174.5 → 176.4 | 2.42 → 2.40 | 110.6 → 109.8 | 221.9 → 221.6 |
| 16 | 258.9 → 258.0 | 9.27 → 9.25 | 424.4 → 423.4 | 656.6 → 661.4 |
| 32 | 551.5 → 550.7 | 33.53 → 33.34 | 1,534.8 → 1,526.0 | 2,239.6 → 2,283.3 |
| 64 | 1,241.3 → 1,187.0 | 93.16 → 94.49 | 4,264.7 → 4,325.3 | 7,199.2 → 7,261.6 |

| RD | Loaded chunks V12 → V13 | Rendered chunks V12 → V13 | Opaque submissions V12 → V13 |
|---|---:|---:|---:|
| 8 | 247 → 241 | 33 → 33 | 12 → 11 |
| 16 | 881 → 881 | 250 → 251 | 46 → 46 |
| 32 | 3,366 → 3,361 | 1,026 → 1,028 | 163 → 161 |
| 64 | 8,855 → 9,077 | 2,212 → 2,301 | 381 → 394 |

Endpoint admission/culling counts vary with asynchronous progress. No distance limit,
frustum rule or graphics quality was reduced. RD64 settles at **13,069 → 13,075 loaded
chunks**, **139.61 → 138.16 million vertices**, **6.24 → 6.18 GiB used terrain buffers**,
and **7.68 → 7.71 GiB buffer capacity**. Settled process working set is **1,562 → 1,532 MiB**.
Sparse traces show peak private commit of **11,719 → 11,955 MiB**, so a lower working set
does not imply lower total committed memory. Driver backing can contribute to this.

Buffer figures sum opaque terrain pages (including unused page capacity) and active
transparent VBOs. They are a geometry allocation proxy, **not measured physical VRAM
residency**, and omit pending retired transparent buffers, textures and other rendering
resources. High RD64 memory cost remains substantial. New terrain does not create a
major measured regression on this hardware, but this does not establish low-end viability
at RD64 or remove the engine's existing teleport spikes.

Raw results: `build/v13-performance-v12/performance/streaming.csv` and
`build/v13-performance-v13/performance/streaming.csv`, with sparse memory/queue traces
alongside. The same binary SHA256 is recorded in both fixture directories.

### Tested binaries

Both configurations embed runtime assets and use the Windows GUI subsystem:

- Full/persistent: `build/installed-release/Release/VoxelFrontier.exe`,
  SHA256 `ED70B47A14CEC1E2C0CA608E426C0541C12408D16E6DA09281D516E3EEFF956B`.
- Stateless demo: `build/standalone-demo-release/Release/VoxelFrontier.exe`,
  SHA256 `705C101DA28128904D667FD0000AB3EAC0CF34F248D5C7FEDD41374B55C06720`.

The full build has `VOXELFRONTIER_STANDALONE_DEMO=OFF`; the demo has it `ON`.
Both were launched with an isolated working/data directory and loaded embedded assets.
The application still reports the existing 2.11.2-beta source version; generator 13 is
the unreleased change. No installer/release artifact is published during this request.

## Reproduction

From the repository root, after building Release:

```powershell
tools/run_aesthetics_validation.ps1 -Output build/aesthetics-validation
# Use the project's Python environment with numpy and Pillow:
python tools/build_density_previews.py build/aesthetics-validation/audit --versions 12 13
python tools/analyze_aesthetics_volumes.py build/aesthetics-validation/audit
python tools/check_generation_hashes.py <baseline-generation.csv> <current-generation.csv> --versions 12
```

The existing executable fixture accepts `--streaming-benchmark=<directory>` together
with `--generator=12` or `--generator=13`. Use an isolated `VOXEL_FRONTIER_DATA_DIR` with an
empty `voxel_settings.cfg` for the full build so legacy user-save import cannot affect a
fixture. The audit script handles this isolation and restores the previous environment.

For real rendered previews, combine `--worldgen-test --preview-clean --generator=13`
with `--seed`, `--preview-x`, `--preview-z`, `--preview-y`, `--preview-yaw` and
`--preview-pitch`. `--preview-ground` selects the local surface if no explicit Y is supplied.
These flags only affect developer previews; ordinary gameplay is unchanged.

## Remaining limits

- Connectivity is guaranteed by density constraints locally and verified over finite
  samples; a whole-world graph connectivity proof is not claimed.
- Visible voxel contours and some broad summits remain. Attached undercuts are present;
  spectacular freestanding arches are not guaranteed along any particular route.
- Physical low-end/dual-core/integrated-GPU hardware was unavailable for these tests.
- High render distances still load many full-height terrain columns and substantial GPU
  geometry. V13 retains the engine's existing streaming and memory policies rather than
  hiding costs by shortening the render distance.
- The approved beta2 prerelease uses the existing full installer and stateless demo packaging.

## Approved beta2 release packaging

Both embedded-asset Windows Release variants were rebuilt with **2.11.2-beta2** metadata.
The actual standalone and installer were tested outside the repository: menu/panorama,
settings, weather, three independent persistent worlds, old generator-11 save loading,
new generator-13 worlds, demo session reset, installer upgrade/repair/clean install,
shortcuts, and uninstall all passed. Original user data was unchanged. The installed
payload hash matches the final full executable. The packaged full executable also
passed the V13 exploration/density, all-biome, concurrent mesh, structure, lighting,
vertical-bound, and versioned save regressions.

Final distribution SHA256 checksums:

- 924c38d7fd9f6a2faf47b8137ba168a9c3b059979f7f023dc75884552392333c  VoxelFrontier-v2.11.2-beta2-Windows-Standalone.exe
- 9ff9c0be9f5f2d7879a45076290d1b85d24acc9c7b0a9c16c336212e0db98353  VoxelFrontier-v2.11.2-beta2-Windows-Setup.exe
