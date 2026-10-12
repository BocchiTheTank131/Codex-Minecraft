# Generator V17: Terrain & Vegetation Refinement

Validated October 12, 2026; approved for stable v2.11.2 publication.
The working tree also contains previously completed V14–V16 refinements.
Stable release metadata is `2.11.2`; new worlds use Generator 17.

## Implementation

V16 combined a continental field at frequency 0.00010 with only 12% coastal
detail. Erosion used frequency 0.00022. This produced coherent but broad terrain.
V17 uses continental frequency 0.00014, 70% broad continental / 30% regional coastal
detail, and erosion frequency 0.00032. These are generation inputs, not smoothing
of the final biome map. The weights do not mean the map is literally 70% V16.

Climate retains its original warp and sample coordinates. Raw temperature,
humidity, macrotemperature and macrohumidity match V16 exactly. Altitude cooling
uses the new surface height; biome selection and weather still share metadata.

Moderate eroded inland relief reuses rolling/crag samples. Coasts, marshes and
river datums take precedence. V15 mountain profiles, density lattice, attached
overhang constraints, caves, aquifers, ores and natural Y256 ceiling remain.
Building still reaches Y319. Changed regional inputs move mountains and change
cave-to-surface intersections in V17, while legacy worlds are unchanged.

An initial candidate lost Mushroom Fields in a sampled seed: stronger coastal
noise correlated with its deep-ocean mask. An independent regional island sample
fixes this. Mushroom Fields remain isolated ocean habitat; mean global sampled
coverage is 0.93% (V16: 0.31%), still uncommon.

Tree thinning rejects one fifth of already-valid woody roots using a separate
world-coordinate hash. Species, root priority, spacing, support and tree models
are unchanged. Rejecting a root cannot admit replacement neighbors. Giant
mushrooms and ice formations are not woody trees and remain unchanged. Removed
root columns are excluded from the later ground-decoration pass.

Bamboo uses a continuous 96-block grove field with deterministic acceptance
probability from 5% to 95%, retaining occasional isolated stalks. Rejected bamboo
does not fall through to grass/flowers. No IDs, save format, shaders, physics, AI,
lighting rules or rendering pipeline changed.

## Maps and rendered evidence

Seeds: 20260917, 123456789, 42. Each has V9/V16/V17 maps at 4,096 and 30,720 blocks,
512×512 samples. All six comparison sheets were visually inspected.

- Maps/raw data: `build/v17-final-validation/audit/`.
- Actual renderer captures: `build/v17-captures/`, paired forest, jungle, plains,
  mountain, coast and island views, supplemented by interior plains/island and
  a Jungle ground view. Initial biome-edge captures are retained.
- Quantitative summary: `geography-summary.json` in the audit folder.

These are finite-window, four-connected sampled footprints. They do not prove
infinite-world topology; thin rivers can alias at 60-block sample spacing.
Global averages across three seeds:

| Metric | V9 | V16 | V17 |
|---|---:|---:|---:|
| Land fraction (%) | 43.68 | 59.10 | 60.16 |
| Connected land footprints | 1,699 | 12.0 | 31.7 |
| Area-weighted land footprint (km²) | 14.04 | 404.57 | 377.35 |
| Area-weighted water footprint, including rivers (km²) | 263.36 | 164.92 | 137.55 |
| Water/land boundary frequency (%) | 18.06 | 1.81 | 2.56 |
| Ocean components, ocean biomes only | 1,719 | 6.3 | 15.3 |

V17 retains recognizable geography and similar land fraction, with more bays,
islands and inland basins. Individual seeds differ: seed 123456789 still has a
large connected land network. Small continents are not guaranteed in every area.

Paired routes with at least 80% land in both versions:

| Length | Pairs | Biome types V16 → V17 | Height std, blocks V16 → V17 |
|---|---:|---:|---:|
| 500 | 69 | 4.55 → 4.06 | 18.44 → 13.81 |
| 1,000 | 62 | 6.58 → 5.90 | 21.60 → 17.18 |
| 1,500 | 54 | 7.81 → 7.31 | 24.42 → 20.37 |

Routes are gentler overall, with slightly fewer biome types. The improvement is
regional geography/coastal variety, not uniformly greater slope or more biome
crossings. Rendered plains retain open basins and connected grassy hills.
Sampled peaks reached Y254 and never exceeded Y256, including vegetation/models.
Across 30 terrain volumes, exposed faces averaged 87,767 → 89,491 (+2%). Tiny
interior detached surface remnants totaled 2 → 4 blocks; this is not a claim of
mathematically zero detached blocks. No rubble fields were observed in captures.

## Vegetation measurements

The production decorator generated actual geometry on identical supported flat
biome areas, with the same seeds/biomes. All 35 biomes were tested. Jungle areas
were 512×512 per seed; other biomes were 128×128. Counts cover 2,457,600 columns
per version. Species selection and proportions are preserved statistically by
an independent rejection hash, not an exact quota for every small patch.

| Material | V16 | V17 | Change |
|---|---:|---:|---:|
| Woody trees | 17,055 | 13,686 | −19.75% |
| Bamboo stalks | 75,428 | 36,312 | −51.86% |
| Other ground plants | 406,147 | 406,147 | unchanged |

Tests also compare individual cells to reject replacement plants, rather than
only checking net totals. These are controlled biome areas, not a guarantee that
routes with changed geography contain exactly 20% fewer trees. Jungle stalk
reductions per seed ranged from 40.9% to 60.6%; local groves can remain dense.

## Compatibility and tests

- Historical generators 1–16: **3,072 hash rows / 1,344 distinct chunks** match
  archived baseline block hashes exactly.
- Six complete V16 map payloads match their prior SHA256 hashes.
- **1,572,864** V16/V17 paired columns have identical raw climate fields.
- All 35 biomes appear for every seed across the combined map extents.
- Repeat/parallel generation, lattice/chunk seams, negative coordinates, upper
  and lower Y edits, meshes, sunlight/block light and natural height bounds pass.
- Structure slope/biome/loot checks pass (577–611 accepted real terrain regions
  per seed); cave entrance plans and underground connection tests pass.
- Worker plans, eight parallel byte-identical meshes, revision/epoch stale-job
  cancellation, teleport queues and exact GPU upload tests pass.
- Versioned save roundtrips pass. A real pre-change V16 save loaded world,
  inventory, player, mobs and settings and passed the Survival regression.
- Commands/content (73 recipes), weather/settings/audio, menu/multiple worlds,
  safe respawn, tooltips, 20-minute clock and Survival fixtures pass.
- Full/demo menu and command tests pass outside the source/build folders with
  embedded assets. Demo persistence remains disabled.

## Performance

Release, i7-14700F / Radeon RX9060 XT, seed 123456789, 1280×720, simulation 8,
Auto streaming, MSAA off, VSync off, clear weather, F3 off, uncapped. Preserved V16
and current V17 were tested serially without concurrent builds/audits. RD8/16/32:
20s warmup, 8s 8× flight, 12s recovery, 20s teleport. RD64 uses only 2s flight/5s
recovery as a regression smoke test. Flight results:

| RD | FPS V16 → V17 | Mean ms | p99 ms | Worst ms | RAM MiB |
|---|---:|---:|---:|---:|---:|
| 8 | 2582.5 → 2753.9 | 0.387 → 0.363 | 1.040 → 0.825 | 14.78 → 14.69 | 274.4 → 246.2 |
| 16 | 912.5 → 924.5 | 1.096 → 1.082 | 4.445 → 4.482 | 26.92 → 28.57 | 317.8 → 318.0 |
| 32 | 323.6 → 326.7 | 3.090 → 3.061 | 6.745 → 6.643 | 21.78 → 24.53 | 588.1 → 584.2 |
| 64 | 123.0 → 123.8 | 8.131 → 8.078 | 20.183 → 19.127 | 23.59 → 20.98 | 1411.1 → 1414.4 |

| RD | Terrain vertices V16 → V17 | Allocated buffer GiB V16 → V17 |
|---|---:|---:|
| 8 | 2,826,228 → 2,703,210 | 0.215 → 0.207 |
| 16 | 10,311,270 → 9,829,122 | 0.741 → 0.706 |
| 32 | 42,014,394 → 41,456,832 | 2.659 → 2.584 |
| 64 | 133,115,238 → 133,884,402 | 7.172 → 7.190 |

Buffer capacity is engine bookkeeping, not driver-measured VRAM. RAM is working
set. Last mesh/upload timing fields are not per-job statistical averages. Single
run small FPS differences are not evidence of a general optimization. Results
are mixed; RD64's existing large geometry footprint remains.

Fixed teleport coordinates produce different geography. RD32 teleport mean
increased **1.843 → 2.873 ms**, p99 3.389 → 4.529 ms, RAM 585.9 → 647.1 MiB and
buffer capacity 1.436 → 2.746 GiB in the changed scene. RD64 teleport mean rose
3.429 → 4.052 ms, with a 248.7 ms worst frame (V16: 228.3 ms). These regressions
are recorded rather than excluded. Flight recovery queues drained at RD8/16/32;
RD64 recovery was 5.52s vs 5.55s baseline. Its short teleport ended with pending
work. This does not establish extended RD64 travel stability.

Generation: 432 builds/version, three seeds and repeated regional chunks. Mean
**3.121 → 3.219 ms (+3.14%)**; median 2.837 → 2.904 ms (+2.36%). Mean allocated
chunk storage 42,297 → 41,785 bytes (−1.21%). One additional regional island sample
is column-cached; no per-block noise or extra geometry pass was added.

Low-end physical hardware was unavailable. Existing low-core worker-plan tests
pass, but hardware performance claims are limited to the measured machine.

## Builds and changed files

Both Release variants compile. The existing `Survival.cpp:1760` int-to-float warning
remains; no new compile errors. The v2.11.2 packaging rebuild uses the same validated code with stable version metadata.
The hashes below identify the pre-release review builds; final artifact hashes are
in the release SHA256SUMS.txt.

- Full: `build/installed-release/Release/VoxelFrontier.exe`
- Stateless demo: `build/standalone-demo-release/Release/VoxelFrontier.exe`

Full SHA256:
`9F583B1B0EE4628B848727A17A30E489F8E43EF4EF914B0508B658B00D18F507`

Demo SHA256:
`22E143954A8904F103D952E30999579DF1B7AFC734F573659D22102EFE0B8840`

V17 files: `src/WorldGeography.cpp`, `src/WorldAesthetics.cpp`,
`src/BiomeVegetation.cpp`, `include/BiomeVegetation.h`, accepted/default versions
in `include/World.h`, `include/Game.h`, `src/World.cpp`, `src/Game.cpp`,
`src/WorldAestheticsValidation.cpp`, `src/WorldDensityValidation.cpp`,
`tools/run_aesthetics_validation.ps1`, `tools/build_density_previews.py`,
`tools/analyze_geography_refinement.py`, `README.md` and this report.

Reproduce with `--aesthetics-audit=<directory> --generator=17`, then
`build_density_previews.py <directory> --versions 9 16 17 --maps-only` and
`analyze_geography_refinement.py <directory>`. Raw CSVs and screenshots stay in
ignored `build/v17-*` folders. Test saves, logs and binaries are not source changes.
