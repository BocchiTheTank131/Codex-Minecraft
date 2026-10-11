# Generator V11 validation

Generator implementation tested 10 October 2026; release packaging verified 11 October 2026.
Application version **2.11.1**; new worlds select terrain generator **11**.

## Design and compatibility

- V11 uses Y **-64..319**, upper bound 320 exclusive, sea level **64**. Generators 1–10
  retain Y **0..255**, sea level **48**, and their original generation branches.
- V10 continentalness and macroclimate are reused. Smaller compatible habitat/humidity
  variations change biome selection without changing the continental ocean mask.
- An immutable, globally aligned density lattice samples six fields at 4×8×4-block spacing.
  Trilinear interpolation combines height bias, mountain/erosion masks, multi-octave rock
  deformation, chambers, connecting tunnels, sparse noodles, pillars and surface ravines.
  Deformation fades near the world ceiling. Ordinary lowlands retain heightmap-like terrain.
- Each worker owns its column halo, density lattice, occupancy halo and ore-roll cache.
  Seven ore probabilities are cached by depth; random cluster rolls are cached by 3³ cell.
- Regional aquifers use deep local water levels instead of filling all caves below sea level.
  Deep lava is a static, luminous source-pool block with contact damage. Flowing lava,
  cooling reactions and a new fluid simulation are outside this implementation.
- Generated chunks store a dense lower prefix, rounded to 16-block bands, and omit unused
  upper air. This is **not** a fully sparse section system: interior air remains allocated.
  Missing upper bands return Air, sunlight 15 and block light 0; edits/light propagation
  extend the prefix as needed. Packed lighting uses the same allocated prefix.
- Existing numeric item/block IDs are unchanged. Eight new IDs are appended to each enum:
  Deepslate, its five ore variants, Bedrock and Lava. Item IDs including None: **135→143**;
  block IDs including Air/states: **152→160**. Native icon atlas **160×240**, runtime **640×960**.
- World edits still use `VXWORLD3` format version 2, with the existing generator field
  determining the vertical index interpretation. No legacy coordinates are shifted.
- Collision, commands, raycasts, meshes, block lighting, weather roofs, ecology, structures,
  spawn clearance and the player void threshold use versioned world bounds.
- Structures reject unsupported ravine cells across their full footprint. Legacy terrain
  callbacks default to supported, with their original height/sea-level limits.

The public reference pages were requested at minecraft.wiki but inaccessible to the browsing
tool in this environment. Primary Mojang descriptions of
[1.18 terrain and cave types](https://www.minecraft.net/en-us/article/caves---cliffs--part-ii-out-today-java)
and [the expanded vertical range and ore progression](https://feedback.minecraft.net/hc/en-us/articles/4414284658701-Minecraft-Caves-Cliffs-Part-II-1-18-0-Bedrock)
informed the design. The implementation and 16×16 artwork are original to this project.

## Reproducible fixtures

Use an isolated `VOXEL_FRONTIER_DATA_DIR`. Full-build test profiles have an empty
`voxel_settings.cfg` to prevent automatic import from the user's installed profile.

```powershell
VoxelFrontier.exe --density-audit=<absolute-output-directory> --generator=11
python tools/build_density_previews.py <absolute-output-directory>
VoxelFrontier.exe --geography-audit=<absolute-output-directory> --generator=10
VoxelFrontier.exe --survival-smoke --seed=90902 --generator=11
VoxelFrontier.exe --command-smoke --seed=90902 --generator=11
VoxelFrontier.exe --menu-smoke --seed=90902
VoxelFrontier.exe --weather-benchmark=<absolute-output-directory> --generator=11 --seed=123456789
```

Generated maps, CSV files, cutaways, logs, save fixtures and runtime screenshots stay under
ignored build folders or isolated temporary profiles. Python dependencies are development-only.

Final raw results:

- `build/v11-final-audit/results`: density maps, generation/ore measurements and save fixtures.
- `build/v11-final-benchmark/performance/weather.csv`: final V11 OpenGL performance.
- `build/v11-baseline/performance/weather.csv`: preserved V10 benchmark.
- `build/v11-legacy-verification/audit/generation.csv`: final legacy hash verification.
- `build/v11-baseline/audit/generation.csv`: preserved legacy baseline.

## Geography and visual inspection

Maps sample **512×512 columns** at 60-block spacing over 30,720² blocks and 8-block spacing
over 4,096² blocks. Connected biome regions use four-neighbor connectivity, without wrapping
map edges. These are sampled-map statistics, not exact block-resolution region counts.

| Seed | V10 regions, 30,720 | V11 regions, 30,720 | V11 regions, 4,096 | Ocean components V10/V11 |
|---|---:|---:|---:|---:|
| 20260917 | 2,595 | **14,992** | 782 | 6 / 6 |
| 123456789 | 2,848 | **12,697** | 125 | 6 / 6 |
| 42 | 2,471 | **11,375** | 205 | 7 / 7 |

All **35 biomes** occur in each seed's combined maps. Macro temperature, macro humidity
and continentalness retain the V10 fields. Ocean percentages/connectivity remain unchanged.
Seed 42 is below the requested 12k–18k range; the count is a geographical target, not a
guarantee for every seed. Many small sampled components remain, alongside large continuous
major biomes. Map resolution affects this measurement substantially.

Labeled side-by-side previews include biome, temperature, humidity, continentalness and
height fields. Six actual generated 512×384 cross-sections and three generated 64×64×384
volumes supplement the heightmaps. Isometric cutaway images show their lower underground
slice; their rasterized striping is a development visualization artifact.

Actual OpenGL captures were inspected at 50% Brightness: mountain surfaces, a surface-open
ravine, a dark cave, a torch-lit cave, a torch-lit Deepslate cave at Y=-42, and an ocean at sea
level 64. Additional final captures cover seed 123456789's underground and seed 42's snowy
mountain, plus the demo's negative-Y cave. These verify rendering beyond the heightmap preview. The generator includes rare
floating rocks; it does not guarantee a visually ideal formation at every coordinate.

Structures were checked against actual V11 terrain across three seeds:

| Seed | Plains villages | Desert villages | Outposts | Pyramids |
|---|---:|---:|---:|---:|
| 20260917 | 205 | 48 | 344 | 29 |
| 123456789 | 251 | 50 | 431 | 20 |
| 42 | 277 | 33 | 347 | 18 |

Checks cover slope, biome, bounds, support and generated loot. The general regression
fixture also exercises smaller structures, mobs and container persistence.

## Underground and ores

48 regional chunks per seed were measured below the surface minus five blocks:

| Seed | Cave Air | Cave Water | Cave Lava | Dry share of cavity space |
|---|---:|---:|---:|---:|
| 20260917 | 131,171 | 11,672 | 3,109 | 89.87% |
| 123456789 | 161,546 | 11,585 | 1,428 | 92.55% |
| 42 | 163,514 | 16,694 | 197 | 90.64% |

This sample contains substantially more dry cave space than fluid-filled space. It is not
a claim that every cave or ocean entrance stays dry. Open ocean water remains independent
of deep regional aquifers. Surface ravines can connect to underground density caves.

| Ore | Distribution | Y range / peak | Maximum small-vein probability |
|---|---|---|---:|
| Coal | triangular | 0..256, peak 96 | 3.4% |
| Iron, underground | triangular | -64..72, peak 16 | 2.6% |
| Iron, mountain | triangular | 80..320, peak 232 | 4.1% |
| Copper | triangular | -16..112, peak 48 | 2.7% |
| Gold | triangular + deep uniform | -64..32, peak -16; extra -64..-48 | 1.2% + 0.3% |
| Diamond | depth-dependent | -64..16, increasing downward | 0.65% |

These are generator probabilities, not exact final ore percentages: clustering, terrain,
other ores, exposed-diamond rejection and rare lodes also matter. Air-exposed diamonds
retain only 25% of candidates. Histograms in `ore-distribution.png` use actual generated
counts divided by sampled solid blocks, grouped in four-Y-level bins. High-altitude bins
with little remaining terrain are noisy; they are not reliable global ore estimates.

Iron/copper lodes are gated to about **1/211 regional cells** and a narrow density contour.
Targeted rare iron samples found 5,044–14,639 connected ore blocks within a 64×64×56 volume.
These are intentionally large rare deposits, not the expected ore abundance of an ordinary
chunk. The initial lode mask was reduced after inspection.

## Exact compatibility and regression results

- **480 legacy chunk rows**, generators **1–10**, match the preserved baseline hashes and
  every non-timing generation field: **zero differences**.
- Existing Item/Block enum prefixes are exact. All **135 old occupied inventory atlas
  cells** are pixel-identical at native and runtime resolution. Existing world-atlas pixels
  are exact. Newly occupied formerly-empty cells account for the atlas extension.
- New eight icons are 16×16 and use only alpha 0/255. Content fixtures validate metadata,
  UVs, Creative entries, tool/drop and furnace mappings, and save/load IDs.
- V11 repeated generation and asynchronous generation hashes match for all sampled chunks.
  Globally aligned lattice samples match exactly across chunk boundaries.
- Generator 1–10 save round-trips retain their original generator, heights and geography.
  V11 negative-Y and Y=319 edits survive save/reload; resulting mesh bounds include both.
- Negative-Y tests cover player landing, raycast, roof exposure, Torch placement/removal,
  Lava emission/removal, 4-HP contact damage with cooldown, and Creative immunity.
- Commands accept negative valid coordinates and reject Y=-65/Y=320 for V11. Existing
  commands, recipe, inventory, mob identity and physics regression checks pass.
- The full gameplay regression passes on a fresh V11 world and on a copied existing legacy
  profile containing world edits, inventory, mobs and seed. User data was not modified.
- Weather regression and Off/Low/Medium/High rendering paths pass, including audio asset
  indexing, roof/biome behavior, commands and existing mechanics.
- Both final Release executables launch outside the source/build directory with embedded
  assets. Full-build menu tests save/reload three independent worlds, including player,
  inventory, terrain edits, mobs, game mode, time and weather. Demo menu tests reset session
  state and create no world, inventory, player, mob, settings or seed files and no saves folder.

Historical generator-8/10 survey fixtures remain explicitly pinned to their historical
algorithms. The separate density audit provides V11 generation coverage; passing an old
survey is not presented as proof of new cave correctness.

## Performance

Machine: Windows, **Intel Core i7-14700F**, **AMD Radeon RX 9060 XT**, **32 GB RAM**.
Both builds use MSVC Release. These are not integrated-GPU measurements.

CPU generation: **432 samples per version**, 48 regional chunks × three repeats × three seeds.
Mean V10 **2.6877 ms**, V11 **2.6374 ms** (~1.9% lower). This small difference is effectively
neutral within timing noise. Mean allocated block data: **65,536→44,714.7 bytes/chunk**
(**31.8% lower**); packed-light storage follows the same prefix. Density/ore temporary memory
exists per worker and is not included in that block-vector size.

OpenGL fixture: same seed `123456789`, camera setup, time 35, 1280×720, simulation distance 8,
uncapped FPS, VSync/MSAA/F3 off, weather graphics Off. Queues settle before four-second
measurement windows. **The terrain itself differs by design**, so this compares the new
generator workload, not identical geometry. RAM is process working set in MiB, excluding VRAM.

| RD | V10 / V11 FPS | V10 / V11 mean ms | V10 / V11 p99 ms | V10 / V11 worst ms | V10 / V11 RAM MiB |
|---:|---:|---:|---:|---:|---:|
| 8 | 4,688 / 2,482 | 0.213 / 0.403 | 0.487 / 0.478 | 9.724 / 2.046 | 225.7 / 202.4 |
| 16 | 1,549 / 954 | 0.646 / 1.049 | 1.015 / 1.174 | 1.531 / 18.883 | 282.8 / 229.0 |
| 32 | 445 / 338 | 2.250 / 2.962 | 2.672 / 3.126 | 19.590 / 19.069 | 645.7 / 466.3 |
| 64 | 157 / 113 | 6.356 / 8.873 | 6.721 / 8.976 | 23.047 / 23.860 | 2,129.9 / 1,530.3 |

| RD | Loaded chunks | V10 / V11 rendered | V10 / V11 vertex count, millions | V10 / V11 render ms | V10 / V11 last mesh ms | V10 / V11 last upload ms |
|---:|---:|---:|---:|---:|---:|---:|
| 8 | 197 | 72 / 84 | 2.275 / 3.716 | 0.201 / 0.391 | 2.264 / 4.849 | 0.023 / 0.039 |
| 16 | 797 | 262 / 288 | 9.331 / 14.310 | 0.631 / 1.028 | 3.820 / 5.504 | 0.194 / 0.070 |
| 32 | 3,209 | 998 / 1,051 | 37.401 / 48.007 | 2.217 / 2.924 | 3.069 / 5.227 | 0.050 / 0.107 |
| 64 | 12,853 | 3,877 / 3,985 | 113.430 / 165.433 | 6.286 / 8.756 | 2.381 / 4.996 | 0.073 / 0.111 |

Vertex counts are the existing F3 terrain-count statistic, not a new estimate of unique
indexed vertices or VRAM. Mesh/upload columns are the last completed operation, **not**
an average across all chunks. Four-second windows are too short to characterize rare
streaming/autosave stalls; worst-frame differences should not be interpreted as a trend.

The first V11 candidate measured **25.52 ms / 39 FPS at RD64** with denser small cave walls.
That regression was investigated: larger, fewer chambers/tunnels and cached ore rolls reduce
generation work and geometry. Final RD64 is **8.87 ms / 113 FPS**, with about **28% less
working-set RAM** than V10, but remains **40% more frame time / 28% lower FPS** than V10.
About 46% more terrain vertices and more expensive meshing explain much of the remaining
cost. This is a material rendering tradeoff, not a performance-neutral overhaul.

Weather Low/Medium/High at RD64 measured 8.865 / 8.871 / 8.869 ms; Clear control was 8.821 ms.
These measurements preserve the existing optimized rendering path; no render-distance
reduction, geometry LOD or unrelated rendering redesign was introduced.

## Remaining limits

- The 12k–18k biome-region target is met by two of the three measured seeds, not guaranteed.
- Only three full geographical/underground map seeds and selected runtime views were inspected.
  Sampling and regression tests cannot guarantee every terrain formation is ideal.
- Low-end/iGPU performance has not been measured. RD64 still has a material GPU/mesh cost;
  extended travel and long-running frame-pacing tests remain worthwhile before publication.
- Trimmed dense-prefix storage saves upper air, not all interior air; it is intentionally
  smaller in scope than a full sparse vertical-section engine rewrite.
- Lava pools are static sources. Water keeps the existing flowing-fluid implementation.

Release builds: `build/installed-release/Release/VoxelFrontier.exe` (full persistence) and
`build/standalone-demo-release/Release/VoxelFrontier.exe` (stateless demo). The existing
packaging workflow produces the v2.11.1 standalone and Setup executables for the approved release.

Pre-release development executable SHA256 values (before stable version metadata was rebuilt):

```text
B03BEDAEFC524A4763174B53BD64D7616D0E1882BB57A9D765D7374D1BD6A964  full
209B9FAE6727C173903D2790E1F2A1E60D2F671E9766F7018EF000FF75836AE6  demo
```

## v2.11.1 release packaging

On 11 October 2026, `package_release.ps1` rebuilt both Release variants with stable
2.11.1 executable/installer metadata and compiled the Inno Setup installer. The final
standalone was tested from a directory containing only that executable. Menu and weather
checks passed, and no save/config/seed files or saved-world directories were created.

The actual final Setup executable passed:

- Upgrade from the previous `v2.11.1-beta` installer, preserving existing world-file hashes.
- Reloading those Generator 10 worlds without changing their generator version.
- Creating/reloading three independent new Generator 11 worlds.
- Weather and density-audit checks in the installed executable, including generators 1–11
  save round-trips, negative-Y behavior and all 35 biomes.
- Repair, shortcut correctness, uninstall, clean installation and subsequent launch.
- Preserving world/settings data through repair/uninstall and leaving the original user
  profile unchanged. The prior shortcut was restored after isolated lifecycle testing.

The installed payload hash matches the final full-game build exactly. Release artifact
SHA256 values:

```text
87f37518696b6646251ecf7d3f822a709a985224f02eaee996a70992cb529652  VoxelFrontier-v2.11.1-Windows-Standalone.exe
9f9b12d2bae1da41d8b79e31b611b4c6bcbacf7ff871733e2572b607e27325ec  VoxelFrontier-v2.11.1-Windows-Setup.exe
```

`dist/SHA256SUMS.txt` contains these same hashes. Build/test directories, saves, logs,
temporary screenshots and packaging intermediates are excluded from the source commit.
