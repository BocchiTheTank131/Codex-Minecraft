# Generator V18: Geography, Rivers & Biome Balance

Validated October 12, 2026. Approved for v2.11.3. New worlds use Generator 18; existing worlds retain their stored generator.

## Changes and causes

V17's broad two-dimensional continental slice retained large land bridges even
when its frequency alone was increased. V18 uses the existing Perlin noise at a
non-lattice vertical slice, frequency 0.00019, with subordinate coastal detail
at 0.00055. The 70/30 continental/coastal blend and a small sea datum adjustment
produce bays, straits and separated landmasses without splitting map components
after generation. The original warp, erosion and climate sample coordinates stay.

The old river field followed a single noise contour, with no drainage hierarchy
or local water elevation. `RiverNetwork` replaces that only for V18:

- Jittered 256-block catchment nodes choose their lowest neighboring continental
  potential. Strict descent prevents cycles; only paths reaching sea are admitted.
- Shared endpoints and curved reaches form tributaries and junctions. Catchment
  area controls width; headwaters taper and downstream widths never decrease.
- Water elevations decrease along reaches, from at most Y100 to sea level Y64.
  Channels carve into terrain with dry shoulders that widen on high slopes.
- Generation fills actual channel columns to their local water level. Frozen
  rivers use shared climate metadata and do not place snow on submerged beds.
- Entrance planning reserves the full bore width near rivers, avoiding flooded
  entrances. Channels carved into dry ground retain river biomes near sea level;
  already-submerged ocean reaches retain ocean biomes.
- Each calling thread owns bounded node/tile caches. There is no per-block
  watershed search, global mutable drainage cache, renderer change or new fluid model.

`BiomeBalance` uses the existing continuous humidity/habitat fields to favor
ordinary Forest/Plains/Taiga over wet variants. Mushroom island eligibility is
narrower within the existing deep-ocean mask. Mountains, peaks, shores, oceans,
and cold variants retain geographical/climate eligibility.

## Geography measurements

Seeds: **123456789, 20260917, 42**. Each version uses identical seeds and map
coordinates. Both 4,096 and 30,720-block windows have 512×512 samples. The table
averages the three larger windows; it measures world area, not spawn probability.

| Metric | V17 | V18 |
|---|---:|---:|
| Area-weighted connected dry land | 377.35 km² | 327.70 km² |
| Connected land including river biomes | 383.11 km² | 331.76 km² |
| Area-weighted ocean component | 140.24 km² | 140.12 km² |
| Land fraction, including river biomes | 60.98% | 58.19% |
| Adjacent land/ocean boundary fraction | 1.39% | 1.59% |
| Mushroom Fields world-area coverage | 0.930% | 0.286% |

Land area decreases **13.2%**, reasonably close to the approximate 300 km² goal.
The weighted ocean-area reduction is negligible: the requested 10–15% reduction
in ocean footprints was **not demonstrated**. Connected oceans and occasional
large continents remain. Per-seed dry land areas vary substantially: V18 measures
329.60, 420.36 and 233.15 km²; Mushroom Fields measure 0.326%, 0.208% and 0.324%.

Components use four-neighbor connectivity and `sum(area²)/sum(area)`. Components
touching map edges are truncated. These are comparable finite-window measurements,
not complete-continent estimates. Rivers are reported separately from ocean
biomes; native-resolution river windows avoid aliasing narrow channels at 60-block
overview spacing. No artificial channel cuts were added to satisfy region counts.

### Biome coverage and exploration

| World-area coverage | V17 | V18 |
|---|---:|---:|
| Forest | 3.705% | 6.021% |
| Plains | 6.326% | 6.193% |
| Taiga | 4.083% | 5.269% |
| Birch Forest | 2.420% | 2.115% |
| Savanna | 5.423% | 4.406% |
| Jungle | 4.605% | 4.887% |
| Dark Forest | 1.078% | 1.299% |
| Desert | 2.276% | 1.844% |
| Swamp | 2.868% | 1.589% |
| Flower Forest | 1.081% | 0.206% |
| Old Growth Taiga | 1.384% | 0.356% |
| Ice Spikes | 0.245% | 0.300% |

All 35 biomes were found across the audit seeds. Across 192 two-kilometer routes,
mean biome variety changes from 6.82 to 6.90; routes with at least half land change
from 8.75 to 8.87. Mean transitions increase from 14.98 to 17.84. This is a modest
exploration-diversity improvement, rather than a large increase in biome count.

### Rivers

Each seed samples a 16,384-block-square drainage window with native perpendicular
channel-width probes. Lengths describe admitted curved reaches, including their
coastal ends; they are not all navigable water lengths.

| Seed | Reaches | Total reach length | Actual tributary junctions | Median width | 95th percentile width |
|---|---:|---:|---:|---:|---:|
| 123456789 | 1,220 | 447.78 km | 125 | 5 blocks | 14 blocks |
| 20260917 | 1,018 | 374.19 km | 110 | 5 blocks | 15 blocks |
| 42 | 1,174 | 434.56 km | 136 | 5 blocks | 19.35 blocks |

About 75–80% of probes are 2–7 blocks wide; 19–22% are 8–24. The remaining probes
include narrow source/coastal transitions; 19 sampled reach centers had no
eligible channel water. Checks cover strictly descending paths, shared endpoints,
nondecreasing downstream width, curved border coverage and actual generated
water/ice in 144 representative chunks. Three 512-block native river windows
and engine captures show branching channels and sloping banks.

## Compatibility and correctness

- Generators 1–17 remain version-gated; saves retain their stored generator.
  No block/item IDs or serialized layouts change. V18 saves need a V18-capable build.
- **1,632 distinct historical chunk hashes** across generators 1–17 match their
  preserved fixtures. All six complete V17 map payloads also match the preserved
  V17 executable's output byte-for-byte, including biome and height fields.
- Raw temperature, humidity, macrotemperature and macrohumidity match V17
  **byte-for-byte** in all six map pairs. SHA256 values are exported by the analyzer.
  Altitude cooling still uses the generated local surface, so changed terrain
  can change effective surface temperature.
- Fixed-habitat decoration outputs match byte-for-byte across all 35 biomes:
  13,686 trees, 36,312 bamboo stalks and 406,147 other plant blocks across
  2,457,600 columns in each version. Geography moves habitat locations; it does
  not change V17 tree thinning or bamboo clustering.
- Repeat/parallel generation, negative coordinates, density lattice borders,
  eight concurrent byte-identical meshes, stale revision/epoch cancellation,
  queue recovery, negative/upper Y edits and lighting pass.
- Real structures, connected dry cave entrances, safe spawn checks, the natural
  Y256 ceiling, weather exposure and generator-version save round trips pass.
- Release command/menu/weather/patch272/survival fixtures pass. Full and demo
  binaries also pass menu/command checks outside the source/build directory;
  three independent full saves retain terrain/player/inventory/mobs/time/weather,
  while demo sessions reset and create no save directories.
- A world set written by the preserved V17 executable loads in the new executable.

The six labeled comparison maps and native river previews were visually inspected,
along with actual engine captures of rivers at bank level and from above,
coastlines, a Mushroom Fields island, forested hills and mountains. Captures use
identical seed/camera settings between V17 and V18. They show geographic changes,
not pixel-equivalence; the new generator deliberately changes new-world geography.

## CPU generation and storage

The final alternating three-seed audit contains 432 chunk builds per version.
Mean generation is **3.261 → 3.312 ms (+1.5%)**; median is **2.927 → 2.931 ms**.
Mean retained block storage is **41,785 → 42,041 bytes (+0.6%)**. The small timing
difference is close to measurement noise. Cache cold-start and terrain differences
remain relevant; these samples are not a worst-case throughput guarantee.

## Rendering, frame pacing and memory

Hardware: i7-14700F (28 logical processors), Radeon RX 9060 XT. Existing Release
streaming fixtures use seed 123456789, 1280×720, simulation distance 8, Auto
streaming, identical camera/time, clear weather, MSAA/VSync off, F3 off and
uncapped rendering. Each run warms for 20 seconds, flies at 8× speed for 8 seconds,
allows 12 seconds of recovery, then tests a teleport for 20 seconds. The brief
RD64 smoke uses only 2 seconds of flight and 5 seconds of recovery.

The preserved V17 binary was measured before changes. Initial V18 measurements
appeared substantially slower. Control reruns found large variation in the
unchanged V17 binary as well; repeated V18 RD8/16 measurements returned to the
original range. Both measurement series are retained, rather than reporting
only favorable runs.

### Initial before/after flight series

| RD | V17 mean ms | V18 mean ms | V17 p99 ms | V18 p99 ms | V17 RAM MiB | V18 RAM MiB |
|---:|---:|---:|---:|---:|---:|---:|
| 8 | 0.229 | 0.461 | 0.561 | 4.054 | 256.4 | 243.3 |
| 16 | 0.984 | 1.809 | 4.455 | 11.927 | 314.0 | 318.0 |
| 32 | 3.009 | 6.793 | 6.719 | 10.124 | 583.4 | 1,406.3 |
| 64 | 8.020 | 20.050 | 19.628 | 20.927 | 1,411.0 | 4,007.5 |

### Repeat controls and final smoke

V17/V18 values below use the control reruns for RD8/16/32. RD64 pairs the later
unchanged-V17 control with the original final-V18 smoke; V18 was not retuned or
rerun extensively at RD64.

| RD | FPS V17 / V18 | Mean ms V17 / V18 | p99 ms V17 / V18 | Worst ms V17 / V18 | RAM MiB V17 / V18 | Render ms V17 / V18 |
|---:|---:|---:|---:|---:|---:|---:|
| 8 | 4,487 / 4,388 | 0.223 / 0.228 | 0.551 / 0.654 | 15.02 / 15.80 | 240.8 / 244.3 | 0.174 / 0.174 |
| 16 | 1,028 / 1,020 | 0.973 / 0.980 | 4.302 / 4.423 | 34.43 / 24.20 | 319.6 / 323.1 | 0.782 / 0.797 |
| 32 | 166 / 181 | 6.025 / 5.533 | 9.236 / 8.321 | 17.38 / 29.40 | 1,287.4 / 1,210.0 | 3.844 / 3.564 |
| 64 | 50.2 / 49.9 | 19.938 / 20.050 | 22.174 / 20.927 | 23.10 / 22.62 | 3,864.0 / 4,007.5 | 18.232 / 18.332 |

RD8/16 differences are small. RD32's apparent improvement is not a demonstrated
optimization gain: the original V17 run was faster than either later run.
Frame time and process working set vary substantially under this streaming
workload. The controls do not support attributing the initial slowdown solely
to V18, but they also do not prove identical performance on every system.

### Geometry at those flight endpoints

| RD | Loaded chunks V17 / V18 | Rendered chunks V17 / V18 | Terrain vertices, millions V17 / V18 | Used terrain buffers GiB V17 / V18 | Opaque submissions V17 / V18 |
|---:|---:|---:|---:|---:|---:|
| 8 | 237 / 243 | 39 / 40 | 2.703 / 2.710 | 0.121 / 0.121 | 17 / 17 |
| 16 | 861 / 861 | 258 / 257 | 9.829 / 9.987 | 0.439 / 0.446 | 67 / 77 |
| 32 | 3,401 / 3,341 | 1,069 / 1,053 | 42.395 / 41.084 | 1.895 / 1.837 | 274 / 295 |
| 64 | 6,112 / 6,327 | 1,359 / 1,419 | 70.370 / 73.601 | 3.146 / 3.290 | 305 / 317 |

Buffer totals are engine-accounted opaque+water used bytes, not total driver
VRAM. Capacity and stage/queue/throughput/upload traces are in the CSV files.
Different new geography, worker completion order and visibility can change
submissions without adding render passes. The short RD64 runs are **not fully
settled worlds** and cannot establish maximum RD64 residency. Their existing
high memory cost remains an important limitation. Renderer/mesher algorithms,
render-distance meaning and quality settings are unchanged.

## Reproduction and outputs

Build both configured Release targets, then run:

```powershell
tools/run_aesthetics_validation.ps1 -Generator 18 -Output build/v18-final-validation-complete
python tools/analyze_geography_balance.py build/v18-final-validation-complete/audit
python tools/build_density_previews.py build/v18-final-validation-complete/audit --versions 17 18 --maps-only
python tools/build_river_previews.py build/v18-final-validation-complete/audit
```

Install NumPy/Pillow into an isolated tool environment if required. Historical
comparisons reuse `tools/check_generation_hashes.py` and existing generation
fixtures. All audits isolate save data with `VOXEL_FRONTIER_DATA_DIR`.

Local outputs are under `build/v18-final-validation-complete/audit`,
`build/v18-captures`, `build/v18-legacy-*`, and `build/v18-performance-v*-rd*`.
They are intentionally excluded from Git, as are saves and executables.
Full and demo executables are in `build/installed-release/Release` and
`build/standalone-demo-release/Release`, respectively, with embedded assets.

## Remaining limitations

- Three overview seeds establish a useful comparison, not a global distribution
  guarantee. Ocean footprint reduction and exact 300 km² land area are not claimed.
- Drainage uses continental potential and carved local channels; it is not a
  rainfall/erosion simulation. Closed basins without a bounded ocean outlet are
  skipped, and highland rivers can retain rocky gorges or block-stepped gradients.
- Overview maps undersample tiny islands and narrow rivers; native previews and
  generated block checks supplement them.
- Physical dual-core or integrated-GPU hardware was unavailable. Low-core worker
  planning tests pass, but measured desktop results cannot establish low-end FPS.
- Existing RD64 memory costs remain; render distance and graphics quality are unchanged.

Release packaging uses the existing standalone demo and persistent installer workflow for v2.11.3.
