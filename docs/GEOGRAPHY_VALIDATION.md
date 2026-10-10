# Generator 10 geography validation

Generator 10 validation for v2.11.1-beta, based on the v2.11.0 codebase. No
item/block IDs or save-record layouts changed. Tests ran on Windows 11 with MSVC Release,
an Intel i7-14700F and AMD Radeon RX 9060 XT. Integrated-GPU performance was not measured.

## Cause and approach

Generator 9's principal continental field used a 1,000-block base wavelength with three
octaves; climate fields had roughly 2,600–2,850-block base wavelengths and relatively strong
finer octaves. Erosion, mountain eligibility and rivers also changed at short wavelengths.
Those independent fields repeatedly split terrain and biome families at the overview scale.

Generator 10 uses a separate column sampler and biome selector:

- Continental base wavelength 10,000 blocks, with 12% smaller coastal detail and a broad warp.
- Temperature/humidity base wavelengths approximately 8,333/7,692 blocks, with weak second
  octaves. The actual climate bands span several thousand blocks and are seed-dependent.
- Regional macroclimate plus restrained microclimate; existing altitude cooling is retained.
- Broad erosion/mountain belts, medium ridges, graded foothills and wide river valleys.
- A rotated, non-integer Perlin slice for long river contours, avoiding lattice-shaped rivers.
- Compatible regional biome families, rare offshore mushroom islands and wet swamp basins.
- An irregular, gentle starter-land fallback only raises unsuitable ocean near the origin.
- Existing cached columns, asynchronous generation, cave/carver/ore/vegetation/structure
  modules and weather metadata continue to be used. No final-map smoothing is applied.

## Maps and distributions

Seeds: `20260917`, `123456789`, `42`. Each v9/v10 comparison covers both a 30,720-square-block
region and a 4,096-square-block region centered on the origin. Maps contain 512×512 samples:
60-block overview spacing and 8-block local spacing. Biome, temperature, humidity,
continentalness and elevation maps are labeled side by side and were visually inspected.

The following are four-connected regions in the sampled overview, **not** exact counts of
every region in the infinite world. Narrow rivers and tiny features can be undersampled.

| Seed | Biome regions v9 → v10 | Ocean regions v9 → v10 | Climate bands v9 → v10 |
|---|---:|---:|---:|
| 20260917 | 30,319 → 2,595 | 1,749 → 6 | 376 → 28 |
| 123456789 | 30,046 → 2,848 | 1,677 → 6 | 387 → 33 |
| 42 | 30,854 → 2,471 | 1,731 → 7 | 397 → 30 |

All 35 biomes were found in each seed's combined survey. At the overview scale, Ice Spikes
occupy about 0.28–0.73% and Mushroom Fields 0.17–0.62%. Ocean-family coverage is about
36–42%, compared with 49–51% in v9. Temperature/humidity changes between adjacent overview
samples are about 70% smaller. Major landmasses, ocean basins, climate regions and mountain
belts are continuous; the local maps retain smaller terrain and river detail.

Iterations corrected missing shallow-water swamp habitat (lily pads), grid-like river
contours, excessive mushroom-island coverage and unnecessarily elevated starter terrain.
Actual rendered previews were inspected for jungle vegetation, rocky mountain slopes and
frozen ocean/icebergs. A starting ocean seed receives the deliberate starter-island fallback.

## Compatibility and correctness

- **432/432** stored baseline chunk hashes for generators 1–9 match.
- Generator metadata/save/load roundtrips for versions 1–10 pass; unexplored chunks retain
  their stored generator's block data.
- Repeated regional requests, reversed requests and parallel chunk generation match.
- Chunk-boundary climate/height checks pass, including negative chunk coordinates.
- Adjacent-column height differences in the overview/local test sites were at most 3 blocks.
  This is a sampled test, not a claim that legitimate cliffs never exceed that height.
- All required biome content was found naturally, including four new wood families, Podzol,
  Mycelium, coarse dirt, packed/blue ice, terracotta, bamboo, ferns, dead bushes, lily pads,
  small mushrooms and giant mushrooms.
- Major-structure footprint/slope, biome, overlap and generated-chest/loot checks pass across
  97×97 structure regions per seed: 669, 777 and 665 valid plans respectively, including
  plains/desert villages, outposts and pyramids. Existing minor-structure algorithms remain
  unchanged; the existing structure/core regressions also pass.
- Climate-based rain/snow/dry routing passes for naturally selected biome sites.
- Historical v9 biome/content/recipe/inventory/world persistence audit passes.
- Existing survival regressions pass on rerun, including collision, combat, lighting/AO,
  meshing, fluids, streaming, crafting, audio and four copied legacy-save files.
- Full/demo executables were launched from a temporary directory with no external assets.
  Full menu tests pass independent-world persistence; demo tests pass session reset and create
  no world/player/inventory/mob/seed/settings files.
- Weather benchmark smoke checks pass, including commands, all 19 weather OGG assets,
  precipitation quality levels, roofs, freezing/melting, lightning and audio routing.
- Both full and demo Release builds compile. The existing Survival.cpp int-to-float warning
  remains; no new compiler errors were reported.

An initial survival run failed one inventory-save assertion (Bookshelf); the unchanged test
passed completely on rerun. The unmodified v2.11.0 baseline also failed an inventory-save
assertion (Acacia Leaves). This intermittent save-fixture failure is not resolved or hidden
by this geography update; no unrelated save-system changes were made.

## Generation performance

Three seeds, 65,536 regional columns each:

| Column sampling | v9 | v10 |
|---|---:|---:|
| Mean time per 65,536 columns | 91.26 ms | 39.85 ms |

Full chunk generation samples 25 coordinates distributed over a 25,600-square-block region
per seed, three repeats each. Warm repeat means use 150 observations per generator:

| Full chunk generation | v9 | v10 |
|---|---:|---:|
| Mean per chunk | 3.016 ms | 2.865 ms |

Column sampling is about 56% faster; complete generation is about **5% faster** in this
regional sample. A separate final run measured 3.010/2.889 ms. These small overall gains
depend on terrain: v10 has substantially more solid terrain (sampled mean surface elevations
61–72, versus about 49–50 in v9), so unchanged cave/ore work processes more vertical blocks.

The worker-local exact Perlin column cache reuses fixed X/Z fractions, lattice hashes and
fixed gradient operands as Y advances, without interpolation. An initial hash-only cache
measured 3.732 ms on the final terrain; caching the constant gradient operands reduced that
to 2.865–2.889 ms, about 23% on the identical v10 workload. **50,622 float samples were
bit-identical**, including negative coordinates, lattice boundaries and signed-zero inputs.
All **480 standard chunk hashes plus 450 regional hashes** and all 12 binary maps matched
the preceding cache implementation. The initial cache also matched the pre-cache chunk
hashes. Storage is fixed stack memory per active column; there are no per-block allocations
or shared mutable noise caches. Generators 1–9 retain the original noise path.

## Rendering and memory

The existing weather fixture uses seed 123456789, fixed origin camera/time 35, 1280×720,
simulation distance 8, brightness 50%, uncapped FPS, VSync/MSAA/F3 off and settled queues.
Four seconds are sampled per quality. These rows use weather graphics Off. Terrain differs
by design between generators, so they are workload comparisons, not identical-scene GPU tests.
RAM is current process Working Set in MiB.

| RD | FPS v9 → v10 | Mean ms v9 → v10 | p99 ms v9 → v10 | Worst ms v9 → v10 | RAM MiB v9 → v10 |
|---:|---:|---:|---:|---:|---:|
| 8 | 2999.9 → 3893.6 | 0.333 → 0.257 | 0.864 → 0.429 | 1.966 → 1.598 | 353 → 291 |
| 16 | 1408.5 → 1462.5 | 0.710 → 0.684 | 0.857 → 0.849 | 1.339 → 1.110 | 380 → 344 |
| 32 | 485.8 → 443.7 | 2.059 → 2.254 | 2.197 → 2.425 | 13.238 → 7.732 | 750 → 711 |
| 64 | 173.4 → 160.1 | 5.769 → 6.245 | 5.875 → 6.355 | 44.107 → 14.980 | 2193 → 2194 |

Loaded chunks are unchanged: 197/797/3209/12,853. Rendered chunks change from
75/268/1005/3893 to 72/262/998/3877. Uploaded terrain vertex counts change from
2.468/8.720/37.553/127.571 million to 2.275/9.331/37.401/113.430 million.
RD64 render time changes from 5.676 to 6.178 ms; last mesh-build/upload samples are
2.424/0.035 ms versus 2.389/0.052 ms. Those last-operation samples are not average meshing times.
RD64 weather Low/Medium/High measured 6.244/6.247/6.251 ms, and Clear control 6.228 ms.

Rendering/RAM behavior varies with which broad region surrounds the camera. More continuous
dense forests can be more demanding than fragmented ocean/land terrain. No graphics quality,
render-distance meaning, vegetation assets or renderer architecture was reduced to hide costs.
These short tests do not establish long-travel leak behavior or integrated-GPU performance.

## Reproduction

## Beta packaging verification

Both v2.11.1-beta variants were rebuilt with embedded assets. The final full executable
passed the geography audit and matched all 480 previously validated chunk hashes.
The actual setup upgraded a temporary v2.11.0 installation; all 21 previous save/settings
files were unchanged before launching the new game. Installed menu/world persistence,
repair/reinstall and uninstall passed, preserving test user data. The actual standalone
package passed clean-folder menu/session tests with zero save/config/seed files created.
Executable display metadata is `2.11.1-beta`; Windows numeric installer metadata is
`2.11.1` so existing upgrade/version comparisons continue to work.

## Reproduction commands

1. Build full and demo with the existing Release CMake configurations.
2. Run `VoxelFrontier.exe --geography-audit=<absolute-directory> --seed=123456789`.
3. Run `python tools/build_geography_previews.py <directory>` (development Pillow/NumPy).
4. Run the historical `--biome-audit`, `--survival-smoke` and `--menu-smoke` fixtures.
5. Run `--weather-benchmark=<absolute-directory> --seed=123456789` serially, with a fresh
   isolated settings/profile directory and no simultaneous build or GPU test.

Local evidence is under `build/geography-final/`, `build/geography-benchmark-final/`
and the corresponding isolated profile logs. Outputs and screenshots are ignored build
artifacts. The beta packages use the same validated geography implementation.

Remaining limitations: rivers are inexpensive contour/valley approximations, not a global
watershed simulation; small closed drainage shapes and fading highland headwaters can occur.
Finite sampled maps cannot prove worldwide ocean connectivity or rule out every tiny biome.
