# Generator V16: Climate Distribution Refinement

Unreleased source work, October 11, 2026. No commit, push, tag or publication.
Executable release metadata remains `2.11.2-beta2`; new worlds use Generator 16.
The working tree includes the previously completed V14 and V15 refinements.

## Cause and implementation

V15's broad macrotemperature used frequency 0.00012, while the additional
0.0013 thermal field had weight only 0.14. It could roughen boundaries but rarely
interrupt a large hot/cold region. Temperature also affected the dry plateau
height blend: changing its input there would unintentionally reshape terrain.

`RegionalClimate.h/.cpp` adds one warped, continuous regional noise sample at
frequency 0.00070. It reuses the existing thermal sample for a restrained domain
warp and retains the unchanged macrotemperature field. The resulting temperature
is `clamp(0.07 + macro*0.86 + regional*0.50 + thermal*0.14 + habitat*0.018)`.
Reducing the finest habitat weight avoids decorating climate boundaries with
numerous tiny category islands. The scales are noise wavelengths, not guaranteed
diameters of biome regions.

Only generators 16 and later use this function, **after all V15 height shaping**.
The original plateau temperature remains in the terrain calculation. Height-only
coastal evaluations skip the new sample. Existing cached chunk/neighbor columns
carry the result; there are no extra per-voxel noise evaluations or render passes.

Biome selection consumes this temperature, and the existing shared
`biomeClimateAt` query supplies precipitation, freezing, color and diagnostic
inputs. Vegetation eligibility follows the selected biome. Altitude cooling,
dry-biome flags and weather thresholds are unchanged. Humidity, macrotemperature,
macrohumidity, continentalness, erosion, weirdness, height, rivers, ravines and
island inputs retain their V15 values. Underlying density/cave/aquifer/ore rules
are unchanged. Climate-dependent surface materials, snow, plants and biome-bound
structures can differ in V16; final block arrays are not expected to match V15.

World metadata accepts 16 without changing save layout or item/block IDs.
Stored generators 1–15 retain their existing algorithms and climate behavior.
World bounds remain Y=-64..319, sea level 64, natural generation ceiling Y256;
the previously implemented 20-minute cycle is unchanged.

## Maps and exploration

Seeds **20260917, 123456789 and 42** were sampled at 512×512 for both
**4,096×4,096** and **30,720×30,720** block extents. The original attached
comparisons were used to identify the desired middle ground. V9/V15/V16 maps
include labeled temperature, biome, humidity, continentalness and terrain fields.

The first candidate was rejected after visual inspection and measurement: its
large-map boundary frequency exceeded V9. Regional weight/frequency were reduced
before final validation. The final maps retain large warm/cold neighborhoods,
with irregular regional pockets and intermediate transitions rather than V9's
many similarly sized isolated temperature lobes.

Across the three 30,720-block maps:

| Metric | V9 | V15 | V16 |
|---|---:|---:|---:|
| Neighbor climate-category boundary frequency | 7.92% | 4.05% | 5.65% |
| Mean temperature difference, 60 blocks | 0.01816 | 0.00960 | 0.01303 |
| Mean temperature difference, approximately 512 blocks | 0.13841 | 0.05891 | 0.10078 |
| Mean temperature difference, approximately 1,024 blocks | 0.20755 | 0.09311 | 0.15521 |
| Mean temperature difference, approximately 2,048 blocks | 0.27136 | 0.16656 | 0.20895 |
| Freezing-to-Hot neighboring sample edges | 0 | 0 | 0 |

At the 4,096-block scale, mean neighbor differences are 0.00238 / 0.00127 /
0.00173 and category-boundary frequency 1.08% / 0.61% / 0.76%. Thus the middle
ground is present at both extents and several separation distances.

V16's large-map macrotemperature correlation is **0.85–0.87**. Median sampled
connected climate-region equivalent widths are **516–610 blocks**, with much
larger retained macroregions. These are square-root-of-area proxies, not direct
transect widths. Raw region count alone is misleading: V15 has 332–424 components
because fine boundary speckles count individually; V16 has 226–241 despite more
frequent meaningful boundaries. Singleton category area falls from V15's 0.063%
to V16's 0.0055%. No requirement was tuned solely to region count.

64 fixed routes per seed, sampled every 8 blocks along X and Z/3; the table
includes paired routes with at least 80% land. Terrain is identical on those routes.

| Route length | Routes | Temperature range V15 → V16 | Climate crossings V15 → V16 | Biome types V15 → V16 |
|---|---:|---:|---:|---:|
| 500 blocks | 85 | 0.063 → 0.108 | 0.47 → 0.61 | 3.85 → 4.20 |
| 1,000 blocks | 78 | 0.119 → 0.198 | 0.95 → 1.13 | 5.38 → 5.96 |
| 1,500 blocks | 72 | 0.164 → 0.259 | 1.36 → 1.71 | 6.46 → 7.24 |

## Correctness

- All 35 biomes available in each seed's combined local/overview sample.
- **1,572,864 paired columns** retain bit-identical exported height, humidity,
  continents, rivers and both macroclimate fields. C++ additionally verifies
  unrounded height, erosion, weirdness, peak/valley, ravine and island inputs.
- Shared biome/weather climate agrees with the new sampled temperature, selected
  biome, humidity, dry flag and separately calculated altitude temperature.
- **1,200 distinct historical chunks / 2,640 checked rows** match recorded
  generator 1–15 hashes exactly, including 144 V15 chunks from the preserved
  pre-change baseline. Legacy generator roundtrips keep their stored version.
- **48 complete historical map payloads** for generators 9–15 match their
  pre-change SHA256 hashes, including temperature, biome and humidity samples;
  the old shared weather classification receives unchanged climate inputs.
- Repeated and parallel chunk generation, negative coordinates, density lattice
  seams, worker epochs/revisions, mesh/upload equivalence, cave entrances,
  structures, safe spawning, negative/upper-Y edits, lighting and persistence
  regressions pass. High-peak feature scans stay at or below Y254 for these seeds;
  generated terrain, complete vegetation and structure bounds respect Y256.

## Performance and builds

CPU generation: 48 fixed chunks × 3 repeats × 3 seeds per version, no concurrent
benchmark/compiler work. Mean **3.08194 → 3.10176 ms** (+0.64%); median
**2.80345 → 2.82365 ms** (+0.72%). Per-seed mean increases are 0.45%, 0.62% and
0.88%. This small cost is consistent with the additional column sample and changed
biome decoration; it is not a material speed improvement or major regression.

Matched streaming fixture: i7-14700F (28 logical threads), Radeon RX 9060 XT,
1280×720, simulation distance 8, Auto workers, MSAA 0, VSync/F3 off, clear weather,
uncapped FPS. Each serial run includes 20-second warmup, 8-second 8× flight,
12-second recovery and a 20-second teleport phase. Table shows the flight phase.

| RD | FPS V15 → V16 | Mean ms | p99 ms | Worst ms | RAM MiB |
|---|---:|---:|---:|---:|---:|
| 8 | 4,550 → 4,600 | 0.220 → 0.217 | 0.392 → 0.399 | 14.43 → 14.82 | 272.4 → 244.0 |
| 16 | 1,007 → 1,021 | 0.993 → 0.980 | 4.420 → 4.354 | 27.74 → 25.34 | 320.9 → 308.2 |
| 32 | 328 → 333 | 3.045 → 3.001 | 6.878 → 6.639 | 33.55 → 16.92 | 587.2 → 589.5 |

These approximately 1–1.5% mean-frame differences are within ordinary run
variation; do not interpret them as a rendering optimization. The single-run
worst-frame difference also is not evidence of a guaranteed pacing improvement.
RAM is process working set, influenced by allocation/timing; storage layout did
not change. RD32 RAM increased approximately 2.4 MiB, while the other runs were lower.

Render CPU time: RD8 0.1673 → 0.1642 ms, RD16 0.8026 → 0.8026 ms,
RD32 1.9888 → 1.9959 ms. Terrain vertices: 2.826M → 2.826M, 10.310M → 10.311M,
42.142M → 42.015M. RD32 retained terrain buffer capacity: 2.657 → 2.648 GiB
(engine allocation accounting, not driver-measured VRAM). Opaque draw submissions
were 17 → 18, 64 → 68 and 277 → 276; page allocation and visible content differ,
with no additional render category/pass. Generation and mesh backlogs drained
during recovery at all three distances. No focused-test regression warranted RD64 tuning.

Both embedded-asset Release variants built successfully:

- Full persistent: `build/installed-release/Release/VoxelFrontier.exe`
  SHA256 `C643C25259A2D3D997EE62FAD28041955AB7AD4AD4706B104209858E4FE8FEF1`.
- Stateless demo: `build/standalone-demo-release/Release/VoxelFrontier.exe`
  SHA256 `448BB1C5B9D8B652E5FE59D4E2E63BFDBFBE34639FC1445BAEA6252FB6A14AA3`.

Full command/content, menu/multiple-world, weather, respawn/tooltip and Survival
smoke fixtures passed. Weather tests include all 19 asset decodes, rain/snow/dry
biomes, roof exposure, hydration/fire, snow accumulation/melting, lightning,
spatial sound, settings quality and metadata roundtrips. Full and demo executables
were copied to `%TEMP%/VoxelFrontier-v16-review/{full,demo}` and independently
passed menu and command/content tests outside source/build directories. Persistent
world separation and the demo's prohibition of world selection/saving remained intact.

A normal saved world produced by the pre-change V15 menu fixture also passed
the full Survival suite after copying its world, inventory, player, mobs and time
metadata into an isolated profile: five existing file loads passed. An additional
synthetic density-audit save loaded correctly but failed free-flight/Pick Block
assertions because its saved starting position is in an underground test enclosure.
The preserved V15 executable reproduced the same two failures on that exact save.
This is a fixture precondition limitation, not a new V16 gameplay regression;
the normal-save and standard clean-profile suites passed without changing physics.

Actual renderer captures at seed 20260917 include ground-level forest, taiga,
snowy forest and desert, plus flying mountain/coast views; forest, mountain and
coast have same-camera V15 pairs. Extra captures inspect a Taiga/Snowy boundary
and a summit with base temperature 0.434 but altitude-cooled surface temperature
below 0.17. Mountains retain their shape; regional surface materials/vegetation
and ocean temperature variants change as expected. These are real framebuffer
captures, not schematic map renders.

## Reproduction and evidence

```powershell
cmake --build build/installed-release --config Release --parallel 8
cmake --build build/standalone-demo-release --config Release --parallel 8
./tools/run_aesthetics_validation.ps1 -Generator 16 -Output build/v16-final-validation
python tools/build_density_previews.py build/v16-final-validation/audit --versions 9 15 16
python tools/analyze_climate_distribution.py build/v16-final-validation/audit
```

Local generated evidence is under `build/v16-final-validation/audit/`: six labeled
comparison sheets, individual field maps, `climate-summary.json`, `climate-routes.csv`,
`short-routes.csv`, `generation.csv`, actual generated volumes and compatibility
saves. These outputs remain ignored and are not source artifacts.
Runtime captures are in `build/v16-captures/`, with site coordinates in
`build/v16-capture-sites.csv` and `build/v16-transition-sites.csv`; frame/queue
data are under `build/v16-performance-v{15,16}-rd{8,16,32}/performance/`.

## Limits

Noise scales do not guarantee a climate change every fixed travel distance; large
oceans, deserts and cold/hot macroregions remain intentional. Sampled maps cannot
prove distribution over every possible seed. Rare maximum local gradients can be
steeper than V9 even though average variation is lower; the final local-map maximum
neighbor change is 0.0115 across 8 blocks, with no adjacent extreme categories.
World-shape preservation refers to unchanged terrain inputs/rules, not identical
trees, ice/snow decoration or biome-dependent structures in newly generated V16 worlds.

Physical low-end hardware and lengthy RD64 testing are not part of this focused
pass. Existing worker-plan tests include dual-core constraints; no graphics quality,
render-distance meaning or streaming policy was reduced to mask costs.
