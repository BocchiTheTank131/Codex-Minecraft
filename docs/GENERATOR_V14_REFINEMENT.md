# Terrain refinement and 20-minute world time

Status: unreleased source changes, October 11, 2026. No commit, tag, push or release
was made. Executable version metadata remains `2.11.2-beta2` until release approval.

This report records the V14 stage. Current source builds subsequently use V15;
see [V15 validation](GENERATOR_V15_VALIDATION.md). V14 saves retain this geography.

## Compatibility decision

Generator V13 already shipped in `v2.11.2-beta2` at
`cd6c6259645efa7b6e35d344a1642cab871a40bc`. Refining it in place would change the
geography of unexplored chunks in existing V13 worlds. Therefore new worlds use
**V14**; saved worlds keep their stored generator, including V13. This extends the
existing version branches rather than replacing the generator.

No block/item IDs, binary world records, renderer buffers, AI or physics were changed.
World height remains Y=-64..319 and sea level 64 for generators 11 and newer.

## Implementation

- Increase mountain ridge frequency from 0.0016 to 0.0024, keeping the mountain
  elevation amplitudes and regional masks. Inland rolling relief reuses existing
  rolling/crag noise and fades into the existing coast and river blends.
- Add smooth regional temperature variation at frequency 0.0013, amplitude 0.14,
  plus local variation 0.04. Macroclimate, humidity, continentalness and river noise
  are unchanged. Biome selection and weather read the same climate sample;
  altitude cooling remains in place.
- Narrow the coastal selection height band. Stony Shores use actual cardinal
  coastal relief sampled 64 blocks away rather than erosion alone. Four bounded
  height-only evaluations run only in eligible coastal columns; they do not recurse.
- Independently retain about half of eligible Flower Forest decorations, preserving
  the original flower-color choice. A smooth deterministic 96-block woodland
  density field creates occasional openings without thinning every forest uniformly.
- Preserve V13's subtractive density constraints, attached overhangs, surface roofs,
  cave/ravine systems, aquifers, ores and existing vegetation models.

Main production files: `WorldAesthetics.cpp`, `WorldGeography.cpp`,
`BiomeVegetation.cpp`, `World.cpp`, `WorldTime.h`, `Game.cpp`, `Renderer.cpp`,
`CommandSystem.cpp`, and `WorldLibrary.cpp`, with associated declarations.
Audit/validation changes are in `WorldAestheticsValidation.cpp`,
`WorldExplorationValidation.cpp`, `WorldTimeValidation.cpp`,
`StreamingValidation.cpp`, and the terrain analysis/validation tools.

## Terrain and climate measurements

Seeds: 20260917, 123456789, 42. Each version exports 512×512 samples covering both
4,096×4,096 and 30,720×30,720 blocks. The large map sample spacing is 60 blocks;
the local map spacing is 8. Widths below are means of connected axis-transect
runs, excluding runs clipped by map edges. They are useful comparative proxies,
not precise measurements perpendicular to every mountain or shoreline.

| Seed | Mountain span above Y180, V13 → V14 | Change | Terrain height p99, V13 → V14 |
| --- | --- | --- | --- |
| 20260917 | 544.73 → 388.03 blocks | -28.77% | 256 → 257 |
| 123456789 | 656.66 → 491.08 blocks | -25.22% | 277 → 280 |
| 42 | 664.69 → 482.39 blocks | -27.43% | 271 → 273 |

The large-map result meets the narrower-range goal without reducing peak elevations.
This is not a guarantee for every individual mountain: the 4,096-block seed20260917
window has a slightly wider mean Y180 run (94.57 → 100.93 blocks), because the
new ridge positions and local relief change which peaks intersect that window.

Local adjacent-height-difference standard deviation increases from 2.840 to 3.006,
0.447 to 0.479, and 0.615 to 0.655 blocks in the three local maps. This is a modest
increase in ordinary terrain variation, rather than uniform extreme terrain.

Mean absolute temperature change over approximately 500 blocks increases by
32–40% on the large maps; variation over approximately 1,000 blocks increases
about 9–12%. Macro temperature/humidity, humidity, continentalness and river fields
match exactly across all six paired maps: 1,572,864 sampled positions per field.
All 35 biomes occur for each seed across the two extents. Ocean component counts
remain 6, 6 and 7. Connected biome regions in the large maps change from
12,966/13,167/10,560 to 15,928/16,574/13,487.

| Seed | Combined coast width, V13 → V14 | Beach incl. snowy width | Stony Shore width |
| --- | --- | --- | --- |
| 20260917 | 779.38 → 346.52 | 678.01 → 427.67 | 338.49 → 130.89 |
| 123456789 | 669.27 → 300.96 | 556.26 → 386.21 | 333.23 → 133.40 |
| 42 | 717.64 → 327.67 | 630.77 → 402.51 | 304.68 → 119.80 |

Combined coast spans shrink about 54–56%; sandy/snowy beaches about 31–37% and
Stony Shores about 60–61%. The two shore types do **not** each shrink exactly 50%.
Actual relief eligibility was retained to avoid making Stony Shores practically absent.

Flower Forest tests run real tree/decoration placement over 65,536 controlled flat
columns per seed, counting resulting blocks rather than placement attempts:

| Seed | Flowers V13 → V14 | Retained |
| --- | --- | --- |
| 20260917 | 35,275 → 17,617 | 49.94% |
| 123456789 | 35,747 → 17,930 | 50.16% |
| 42 | 35,405 → 17,723 | 50.06% |

Both flower colors remain. Leaf counts are unchanged in the first fixture, and
decrease 17.24% and 15.39% in the others; clearings depend on region rather than
applying a universal canopy reduction.

Land-route mean biome counts at 500/1,000/1,500 blocks change from
2.89/4.35/5.26 to 2.91/4.44/5.32. This is a small diversity improvement, not a
dramatic increase. Thirty paired sampled 96×96×384 volumes contain 7 → 6 detached
surface rock blocks, 3,225,778 → 3,117,225 exposed rock faces (-3.37%), and
74,276,864 → 71,626,752 block-storage bytes (-3.57%). These samples do not prove
that the entire infinite world contains zero detached blocks.

## World time

The full cycle is 1,200 seconds. Positive and negative solar elevation each occupy
600 seconds, retaining the existing smooth dawn/dusk curves and solar phase offset.
Sky, sun/moon, stars, world lighting and time commands use the same cycle constant.

Commands still use seconds:

| Named time | Seconds |
| --- | --- |
| day | 100 |
| noon | approximately 233.155 |
| night | 700 |
| midnight | approximately 833.155 |

Noon and midnight now coincide with the retained solar angle's extrema. Numeric
`/time set` and `/time add` wrap at 1,200, including negative arguments.

An optional `TIME_CYCLE1 1200` line is appended after existing weather/fire metadata.
Missing markers mean legacy 420-second units: the old fraction of a cycle is mapped
to the new duration. Subsequent reloads do not convert it again. Time units are
independent of terrain generator versions, so old geography is preserved while its
saved time-of-day phase remains equivalent. Weather state/timers keep their existing
units and persistence; hostile spawning still uses the existing daylight rules.

Tests cover three complete accelerated cycles in 0.1-second steps, exactly 600-second
halves, solar extrema, old phases, repeated save/load, malformed markers, numeric
wrapping and command permissions. These are accelerated tests, not a 20-minute
wall-clock play session. Actual rendered captures also exercise six solar phases.

## Validation evidence

Local generated artifacts are deliberately ignored rather than committed:

- `build/v14-reviewed-validation/audit`: paired maps, generation/route/flower/
  connectivity CSVs and `refinement-summary.json`.
- `build/v14-captures` and `build/v14-detail-captures`: actual game screenshots
  of mountain, jungle/forest, savanna, taiga, plains, snowy coastline, decoration
  and solar phases. Several fixed preview heights intersect changed terrain, so
  those views are qualitative evidence, not pixel-equivalence comparisons.
- `build/v14-final-captures`: additionally inspected warm shoreline, ground-level
  jungle valley/cave walls and ground-level rocky mountain slope views.
- `build/v14-regressions`: command, menu, weather, respawn/tooltip and survival logs.

The maps and rendered terrain were inspected. Connected rock slopes, smaller
valleys, steeper ridges and lush hills remain. Some broad summits and voxel terraces
remain intentionally; not every hill is narrow or rugged. The four supplied images
guided terrain proportions, without changing textures, shaders or lighting quality.

Passed: all-biome availability, repeat/parallel generation, density lattice seams,
negative/upper Y block edits and meshes, lighting, versioned save roundtrips,
eight parallel byte-identical meshes, stale revision/epoch cancellation, teleports,
bounded queues, GPU upload checks, connected dry cave entrance bores and structure
terrain/slope/biome/loot checks across three seeds. The entrance audit's negative
coordinate conversion was corrected to floor, matching the existing generator;
the cave-generation algorithm itself was not changed.

Focused Release tests also pass parser/chat/time commands, 73 crafting recipes,
furnaces/fuel, shapes, inventory persistence, three independent worlds, menu/settings,
weather commands and all 19 weather sound decodes, spatial audio, precipitation
qualities, safe spawn/respawn and tooltips. No unrelated production systems were rewritten.

Legacy comparisons are exact: generators1–10 match480 distinct baseline chunk
hashes, generator11 matches432 rows/144 distinct chunks, and generators12–13 match
864 rows/288 distinct chunks. The current V14 audit also reproduces432 V13
baseline rows/144 distinct chunks. All comparison tools reject even one block
difference; old-version save/load tests preserve their stored generator.

Final embedded executables were copied to `%TEMP%/VoxelFrontier-v14-review` and
launched from there without asset folders. Both full/demo menu and command tests
pass. Full saves three independent worlds; the demo restarts a fresh session and
creates no world/player/inventory/mob save files. Test-only screenshots/logs and
an explicitly created empty settings fixture are not persistent gameplay state.

## Performance and builds

Paired chunk generation: 432 timing rows per version, mean 3.065 → 3.272 ms
(+6.74%); median 2.771 → 2.957 ms (+6.71%). The added coastal height queries and
temperature sample have a small measurable cost. Lower sampled exposed geometry
does not imply every scene uses less GPU memory.

### Matched streaming comparison

Hardware: i7-14700F, 28 logical processors, Radeon RX 9060 XT. Preserved published
V13 full executable versus final V14 full executable, run serially. Seed123456789,
1280×720, simulation distance8, clear weather/weather graphicsOff, MSAAOff,
VSyncOff, uncapped, F3Off; same initial solar phase (legacy35/420 versus100/1200).
Existing streaming fixture: 20-second warmup, eight seconds of maximum Spectator
flight, 12 seconds recovery, 20 seconds after teleport. RD64 uses only two seconds
flight and five seconds recovery. No other builds or generation audits ran concurrently.

| RD | FPS V13 → V14 | Mean ms | p99 ms | Worst flight ms | Process RAM MiB |
| --- | --- | --- | --- | --- | --- |
| 8 | 3666 → 3944 | 0.273 → 0.254 | 1.747 → 1.734 | 18.75 → 17.96 | 188.7 → 165.8 |
| 16 | 832 → 846 | 1.202 → 1.181 | 4.353 → 4.372 | 18.50 → 17.97 | 226.4 → 229.8 |
| 32 | 267 → 266 | 3.741 → 3.754 | 6.428 → 6.481 | 16.69 → 16.62 | 512.2 → 513.3 |
| 64 smoke | 108.8 → 109.5 | 9.190 → 9.133 | 18.074 → 16.142 | 22.18 → 19.98 | 1202.4 → 1197.0 |

These are uncapped automated-fixture results, not promised normal-play FPS or
isolated GPU timings. Single-run differences, especially RD8's tiny absolute
times and memory variation, do not establish an optimization gain.

| RD | Render CPU ms V13 → V14 | Total CPU % | Used terrain buffer MiB | Allocated terrain buffer MiB |
| --- | --- | --- | --- | --- |
| 8 | 0.232 → 0.214 | 8.24 → 8.32 | 124.1 → 127.6 | 235.9 → 227.9 |
| 16 | 0.963 → 0.952 | 9.03 → 9.29 | 462.8 → 469.4 | 806.5 → 790.4 |
| 32 | 2.358 → 2.363 | 15.88 → 16.92 | 1976.6 → 1967.5 | 2834.0 → 2805.0 |
| 64 smoke | 5.769 → 5.696 | 13.30 → 13.06 | 5637.6 → 5596.3 | 6696.9 → 6703.5 |

CPU percentages are normalized across all28 logical processors. Buffer counters
are engine terrain-buffer bookkeeping, distinct from process working set and not
a hardware VRAM-residency measurement. More detail does not reduce geometry in
every scene: used terrain bytes increase slightly at RD8/16.

Flight backlog drains in 0.17 → 0.20 seconds at RD8, 0.15 → 0.22 at RD16, and
0.22 → 0.23 at RD32. RD64 does not drain within the short recovery window in
either build; its starting world is still streaming. Recovery mean frame time at
RD32 increases 3.382 → 3.466 ms (+2.5%), and RD64 9.669 → 9.709 ms (+0.4%).
No material sustained frame-time or RAM regression was observed in this sample.

Teleport worst frames are noisier: RD32 26.11 → 50.79 ms and RD64
287.83 → 237.60 ms. These spikes remain visible limitations. Existing main-thread
terrain retirement has a 22.52 ms maximum in the V14 RD32 teleport phase, and
41.84 ms at RD64. The solver, streaming scheduler and buffer retirement algorithm
were not changed; this run does not establish that every new-world teleport is
faster or that rare frame spikes are fixed. RD64 recovery reaches approximately
8.14 GB of allocated terrain buffers, versus8.06 GB before, so its existing
high-distance memory cost remains. No extended RD64 engine optimization was added.

Raw output: `build/v14-performance-v{13,14}-rd{8,16,32,64}/performance/` and
`build/v14-performance-summary.csv`. Low-end physical hardware is not available;
no claim of verified integrated-GPU performance is made.

Both Release configurations compile successfully with embedded assets and static
runtime: `build/installed-release/Release/VoxelFrontier.exe` and
`build/standalone-demo-release/Release/VoxelFrontier.exe`. The latter retains the
build-time stateless-demo flag. Existing compiler/dependency warnings remain; no
new build errors were accepted. Published distribution files are not overwritten.

SHA256 of the tested local Release executables:

- Full: `A4301FB7A8472B5AA98A8113DCAE86E1E7A6E98AF70C610C68BC73463A02BCA4`
- Demo: `1EBF1F1B8615CEE5C4D4378D827B1BD8B21136B4ACF74B19A46644E11D869696`

The preserved V13 benchmark executable has SHA256
`C1ABEBE5E1F4267B262696FBF8049E31A72400C9BF9691D64136E2E9FC04A72F`.
No installer or GitHub assets were republished for these unapproved source changes.
