# Generator V15: Controlled Chaos

Unreleased source work, October 11, 2026. No commit, push, tag or publication.
Executable version metadata remains `2.11.2-beta2`; new worlds use Generator 15.
The working tree also contains the previously completed V14/time refinement.

## Inspection and implementation

V14 combined a large continental height contribution with mountain uplift across
most of a regional mountain mask. Increasing ridge frequency alone still left wide,
high mountain masses. V15 changes this column-level shaping rather than adding an
independent layer of positive 3D noise or smoothing a finished biome map.

- `TerrainChaos` shapes narrow signed-noise contour ridges and local mountain
  groups. The existing continentalness, erosion, drainage and mountain masks still
  control where they belong. Eroded lowlands remain relatively gentle.
- Existing rolling and crag samples provide stronger medium relief. Ridge frequency
  is 0.004, rolling frequency 0.003 and crag frequency 0.0065, with their existing
  octave combinations. A single additional 0.018 noise sample per column supplies
  restrained small rock detail. No extra per-block multi-octave terrain noise was added.
- The continental height contribution is smaller, so local crests rise above
  foothills instead of sitting on uniformly uplifted highlands. Warm dry plateaus,
  river-bank blending, coastal/marsh datum and rare ocean islands remain.
- Compatible habitat noise changes from frequency 0.0045 to 0.006. V14's medium
  temperature field and altitude cooling remain; all 35 biomes and shared weather
  climate queries are retained.
- The V13 subtractive density envelope is unchanged. Density does not add detached
  positive blobs above the surface. Existing undercuts, cave roofs, entrance masks,
  ravines, aquifers and ore rules remain in the same pipeline.

### Natural height ceiling

Above Y200, the continuous compression is `200 + 55 * (1 - exp(-(h-200)/55))`.
Its derivative is continuous at the shoulder and it approaches Y255 smoothly.
Snow may occupy Y256. There is no flat summit produced by clipping high raw peaks.

Natural feature writes use exclusive ceiling 257. Trees require room for their
complete model and are omitted at unsuitable high roots; they are not sliced off.
Structure suitability additionally checks its exact top bounds, using a V15-only
metadata flag and the existing cached footprint query. Upper allocation bands end
at 272 for generation, while empty bands are still trimmed normally.

World coordinates remain **Y=-64..319**, sea level **64**. Player edits at Y319
still allocate, mesh, light and persist correctly. No item/block IDs changed.

### Compatibility

Every new algorithm branch is restricted to generator 15. World metadata accepts
15 and keeps the stored version when loading. Generators 1–14 retain their previous
height, density, vegetation and structure behavior, including unexplored chunks.
V14's 20-minute cycle and legacy time-phase migration are retained without further
changes. See [V14 refinement](GENERATOR_V14_REFINEMENT.md) for that foundation.

Production changes for V15: `TerrainChaos.h/.cpp`, `WorldAesthetics.cpp`,
`WorldDensity.cpp`, `World.cpp`, `World.h`, `Game.h/.cpp`, `Structure.h/.cpp`,
and source registration in `CMakeLists.txt`. Validation additions are in
`WorldDensityValidation.cpp`, `WorldAestheticsValidation.cpp`,
`build_density_previews.py`, `analyze_terrain_chaos.py`, and the audit launcher.

## Maps, routes and visual review

Seeds: **20260917, 123456789, 42**. V9/V14/V15 maps have 512×512 samples covering
both **4,096×4,096** and **30,720×30,720** blocks. Fields: biome, temperature,
humidity, continentalness and terrain height. V9 is shown at its original sea datum
and vertical limits; its terrain is not shifted to imitate newer worlds.

Macro temperature, macro humidity, continentalness and river fields compare
**bit-identically between V14 and V15** in every exported sample. Ocean connected
region counts stay 6/6/7 across the three seeds; V9 has 1749/1677/1731. Thus the new
local variation does not reintroduce V9's fragmented continental geography.

The 60-block large-map sampling counts more biome components in V15:
25,825 / 31,751 / 24,215 versus V14's 15,928 / 16,574 / 13,487. Narrow altitude
bands and habitat variants contribute to this increase. This metric is not a
region-size target and cannot by itself describe walking-scale exploration.

Widths below are mean connected X/Z transect runs above Y180, excluding runs
clipped by map edges. They are comparative footprint proxies, not perpendicular
mountain-width measurements. Smaller footprints and the Y256 ceiling are intentional.

| Seed | V14 span above Y180 | V15 span | V14 map maximum | V15 map maximum |
|---|---:|---:|---:|---:|
| 20260917 | 388 blocks | 100 blocks | 296 | 253 |
| 123456789 | 491 blocks | 109 blocks | 299 | 253 |
| 42 | 482 blocks | 108 blocks | 299 | 252 |

Routes start at 64 fixed locations per seed. The table filters routes with more
than 80% land; each version has 85/78/72 qualifying routes by length. Sampling is
every eight blocks in X with Z advancing by X/3. Slopes use the X projection.
Local crests use a seven-sample window and at least four blocks of prominence
above one end; they are a repeatable ridge-frequency proxy.

| X-projected route length | Biome types, V14 → V15 | Height standard deviation | Local crests |
|---|---:|---:|---:|
| 500 blocks | 2.91 → 3.85 | 11.75 → 15.91 blocks | 0.54 → 1.07 |
| 1,000 blocks | 4.44 → 5.38 | 17.04 → 18.75 blocks | 1.23 → 2.45 |
| 1,500 blocks | 5.32 → 6.46 | 22.74 → 21.55 blocks | 1.93 → 4.01 |

Mean route slopes rise from 0.156/0.169/0.171 to 0.314/0.322/0.332. The share
of samples rising or falling at least one block per X block rises from about
2.7–3.1% to 9.8–10.2%. Long-route height deviation is slightly lower because the
largest mountain masses were reduced; shorter routes have more relief and crests.
These are samples, not a promise of a biome change every fixed walking distance.

Actual OpenGL captures were reviewed at matched flight cameras for jungle/forest
hillsides, rocky summits, savanna hills and taiga. Additional paired views cover
temperate lowlands, plains, savanna, shore regions and underground terrain. Flight
views show compact connected crests and deeper adjacent valleys instead of broad
rounded slopes. Lowland views confirm that flat building areas still exist.
Ground cameras use each version's own surface height; flight camera coordinates
are identical. Cave diagnostics also use a temporary fixture torch for visibility.

The earlier attached references and web references were used for landform
composition, not copied textures or shader changes. Minecraft's official
[1.18 terrain preview](https://www.minecraft.net/en-us/article/new-world-generation-java-available-testing)
provides the functional mountain/cave context. Existing Voxel Frontier lighting,
textures, vegetation models and rendering appearance remain unchanged.

### Connectivity and geometry

Thirty paired 96×96×384 actual generated volumes include ten locations per seed,
including fixed historical artifact sites. Boundary-connected components are
reported separately; no local component-deletion pass is used.
Near-surface components have a top above Y64 and some block within 24 blocks of
the column envelope; this includes upper cave fragments as well as exposed rocks.

| Metric | V14 | V15 |
|---|---:|---:|
| Exposed block faces | 3,058,521 | 2,948,354 (-3.6%) |
| Detached near-surface blocks/components | 9 / 9 | 2 / 2 |
| Undercut columns | 143,637 | 93,190 |
| Generated block storage in these volumes | 73,363,456 bytes | 63,414,272 bytes (-13.6%) |

Undercut counts include caves and ravines; they are not an exact count of dramatic
surface arches. Two isolated near-surface single blocks remain in the sample, so
this is not a claim of zero floating blocks throughout an infinite world. No broad
floating pancakes or debris fields appeared in the inspected views. Fixed synthetic
Flower Forest habitats produce exactly the same flowers/leaves/logs as V14,
preserving its earlier flower reduction and deterministic clearings.

## Correctness and builds

- Exact historical block hashes: **2,208 matching rows / 1,056 distinct chunks**
  across generators 1–14. Comparisons use the preserved historical fixtures,
  including 480 distinct chunks for versions 1–10.
- All 35 biomes occur for each seed in the combined exported maps.
- Repeat and parallel chunk generation, lattice seams, negative coordinates,
  cache eviction and eight parallel byte-identical CPU mesh results pass.
- Worker plans, streaming priority, teleport epochs, stale revision cancellation,
  bounded queues and exact main-thread GPU upload checks pass.
- Every block in the 30 V15 connectivity volumes is checked against the natural
  ceiling. Another 25 high-peak chunks per seed include vegetation and exact
  structure bounds. The highest natural block in these peak neighborhoods is Y254.
- Synthetic high-root Jungle tests cover roots Y231, 232, 240 and 255, checking
  complete tree clearance and no plant writes above Y256. The compression curve
  is tested for continuity, monotonicity, finiteness and its ceiling.
- Y=-64/Y319 edits, meshes, lighting, inventory/block persistence and generator
  version save/load roundtrips pass. Real structure suitability/loot tests pass
  for villages, outposts and pyramids; tested seeds produce 558/588/555 plans.
- Cave entrance connectivity and surface-versus-camera climate checks pass.
- Full and standalone Release configurations compile. The existing `Survival.cpp`
  int-to-float warning and dependency CMake deprecation warnings remain.
- Command/content, menu/multiple worlds, weather, patch272/respawn/tooltips and
  survival smoke fixtures pass. Time tests cover three complete simulated cycles,
  equal 600-second halves, old phase migration, commands and reload. Weather tests
  cover 19 audio decodes, roofs, biome precipitation, lightning, settings and persistence.
- Both final binaries launch from isolated folders outside the source/build tree.
  Menu and command fixtures pass for full and demo builds, including persistence
  enabled/disabled behavior and embedded assets.

## Performance

Preserved V14 binary: SHA256
`a4301fb7a8472b5aa98a8113dcae86e1e7a6e98af70c610c68bc73463a02bca4`.
Final full V15 binary:
`204b65dda031cf88b9ffc0fa234ba4f0fbc1594b3617528c98d8af88bf4f1059`.
Final demo V15 binary:
`b86d8c0fa1635fb2c10b9946e440c3f06b095bee54bbdd99f3b738d375da2561`.

The paired generation audit has 432 rows per version: 48 regional chunks × three
seeds × three repeats. Mean generation is **3.188 → 3.074 ms/chunk** (about 3.6%
lower); median **2.871 → 2.792 ms**. Lower generated heights/allocated bands offset
the extra column noise sample in this set. This is a small result, not an engine
throughput overhaul.

Streaming runs execute serially on an i7-14700F / Radeon RX9060XT, 1280×720,
seed 123456789, identical camera path/time, simulation distance 8, Auto streaming,
VSync off, MSAA off, weather Clear/graphics off and F3 off. Each has 20 seconds of
warm-up, eight seconds of 8× spectator flight and 12 seconds of recovery, then a
teleport. RD64 uses a short two-second flight/five-second recovery; it is a bounded
regression check rather than an extended flight test. These are the existing
fixture settings for both versions, not changes to game graphics defaults.

| RD | Flight FPS, V14 → V15 | Mean frame ms | p99 frame ms | Process working set MiB |
|---|---:|---:|---:|---:|
| 8 | 4320 → 4567 | 0.231 → 0.219 | 0.425 → 0.394 | 266.1 → 257.4 |
| 16 | 973 → 993 | 1.027 → 1.007 | 4.397 → 4.567 | 318.5 → 318.1 |
| 32 | 326 → 328 | 3.070 → 3.045 | 6.687 → 6.900 | 591.9 → 585.5 |
| 64 | 122 → 125 | 8.205 → 8.029 | 20.197 → 18.501 | 1424.1 → 1430.4 |

Mean times are similar. Small p99 regressions at RD16/32 and different isolated
worst frames prevent claiming a universal frame-pacing improvement. Very high
uncapped fixture FPS at small RD does not represent normal Survival FPS.

| RD | Terrain vertices (millions), V14 → V15 | Allocated terrain-buffer capacity GiB | Last mesh job ms |
|---|---:|---:|---:|
| 8 | 2.787 → 2.826 | 0.223 → 0.215 | 6.287 → 5.232 |
| 16 | 10.254 → 10.310 | 0.772 → 0.741 | 4.787 → 4.651 |
| 32 | 42.980 → 42.142 | 2.743 → 2.658 | 4.123 → 4.244 |
| 64 | 136.338 → 135.536 | 7.315 → 7.292 | 4.299 → 4.485 |

The last-job timings are not averages across equivalent mesh jobs. Buffer capacity
is the engine's allocated GL terrain-buffer estimate, not a driver measurement of
physical VRAM. CPU working set is not the same as those GPU allocations. Geometry
slightly increases at RD8/16 and decreases at RD32/64 in this camera route.

Settled/recovery RD64 buffer capacity is still **8.127 → 8.058 GiB**, and process
working set **1581 → 1579 MiB**. Existing high-RD memory costs remain substantial;
there is no memory-quality downgrade to hide them. Teleport cleanup still causes
occasional large spikes in both versions. The repeated RD32 pair reverses the first
41.6 → 73.4 ms worst teleport-frame result: **72.4 → 32.6 ms** on repeat. Repeat
teleport mean times are **1.681 → 1.719 ms**, p99 **2.998 → 3.170 ms**. This does
not show a consistent new worst-frame regression, but does not eliminate existing
spikes or prove their cause. Full raw phase results and stage timers are retained.

## Reproduce and inspect

```powershell
cmake --build build/installed-release --config Release --parallel 4
cmake --build build/standalone-demo-release --config Release --parallel 4
powershell -ExecutionPolicy Bypass -File tools/run_aesthetics_validation.ps1 -Output build/v15-final-validation
python tools/build_density_previews.py build/v15-final-validation/audit --versions 9 14 15
python tools/analyze_terrain_chaos.py build/v15-final-validation/audit
```

Local evidence is under ignored `build/` directories:

- `v15-final-validation/audit/`: raw maps, labeled comparison panels, CSV/JSON
  metrics, block cross-sections, ore histograms and underground volume previews.
- `v15-legacy-{geography,density,exploration}/`: fresh versions 1–14 hash evidence.
- `v15-captures/` and `v15-ground/`: actual paired OpenGL captures, isolated
  settings and exact fixture arguments/logs.
- `v15-performance-v{14,15}-rd{8,16,32,64}/`: serial streaming measurements.

Temporary maps, screenshots, fixtures, save profiles and binaries are not tracked.
The installer/release pipeline and existing published artifacts have not been changed.

## Remaining limits

Bounded tests and visual sampling do not prove every possible seed or landform.
The high-end local GPU is not a substitute for integrated-GPU testing. RD64 still
has substantial pre-existing terrain-buffer costs; this pass does not change
streaming architecture, render distance, graphics quality or terrain storage format.
