# Generator V22: Geographic Diversity

Release validation completed 2026-10-14 for v2.13.2. V22 is the default for newly created worlds; existing worlds retain their recorded generator.

## Causes and implementation

V21 raises continentalness around the origin with a radial starter mask spanning roughly 400-1,000 blocks, modulated by weak coastal noise. An ocean origin consequently becomes a rounded, mostly uniform starter island. The regional continental wavelengths and origin-only 48-block spawn search compound long exploration stretches. V18 drainage also rejects inland sinks wholesale, while old entrance planning checks air clearance without proving a supported landing.

- **Geography:** V22 uses continental/coastal scales .00032/.00090 and a 68/32 blend, with erosion at .00048. It removes the radial land modification. Climate's sampling domain remains unchanged.
- **Habitats:** one regional habitat sample introduces compatible forest/plain/taiga/jungle variations. Geographical and rare biomes retain their eligibility rules. Vegetation placement/density code is unchanged.
- **Waterways:** a separate V22 drainage graph uses 192-block cells, more spring-fed tributaries, continuous shared junctions, additional meanders, and rare lowland lake sinks with incoming streams. Strictly decreasing drainage potential prevents cycles/uphill reach endpoints. Lakes/river valleys are carved into native columns; fluid simulation is unchanged.
- **Entrances:** bounded region plans choose native cavities with supported approaches and landing beds. The terminal chamber preserves the checked floor; its previous enlarged ellipsoid could otherwise remove that floor. Roof/river/structure exclusions and exact post-decoration masks keep the bore clear. Existing cheese/spaghetti/noodle caves, aquifers and density profiles remain intact outside these approaches.
- **Survival spawn:** 289 candidates normally, up to 1,008 additional coarse candidates for ocean-heavy origins, at most eight detailed candidates, and a bounded last-resort natural-land search. Ranking considers land, resources, local relief and compatible biome variety; a nearby cave completes selection. No starter island is constructed. Cached spawn regions are collision-checked again on respawn; saved player positions are retained. Creative/Spectator use the established origin-neighborhood safety search, with natural-land fallback when the origin has no safe land.
- **Cost control:** per-worker entrance, drainage, and native-column caches are bounded. The native-column cache retains at most 512 columns and exact density decisions, roughly 0.23 MiB per calling thread including container overhead. It has no pointers to expired worlds and is keyed by seed/version. OpenGL, meshing, LOD, occlusion and upload algorithms are unchanged.

Generators 1-21 retain their algorithms, including their starter masks and unexplored geography. The existing save header already stores a generator number; accepting 22 requires no new record layout or IDs. Player, mob, weather, item and block IDs are unchanged.

## Geography and exploration results

Seeds: **20260917, 123456789, 42**. Comparisons contain V9/V18/V21/V22 at **4,096 / 15,360 / 46,080 blocks**, each sampled on a 512×512 grid. Nine comparison sheets and all raw fields are in `build/v22-validation/cached/`. Selected sheets:

- [46,080-block comparison, seed 123456789](images/geography-v22/123456789-46080.png)
- [4,096-block comparison, seed 20260917](images/geography-v22/20260917-4096.png)
- [15,360-block comparison, seed 42](images/geography-v22/42-15360.png)

Three-seed averages at 46,080 blocks:

| Measurement | V21 | V22 |
|---|---:|---:|
| Non-ocean area | 54.93% | 54.83% |
| Area-weighted connected land region | 884.89 km² | 675.70 km² |
| Area-weighted connected ocean region | 251.12 km² | 179.52 km² |
| Largest ocean / total ocean area | 41.31% | 33.12% |
| Sampled coastline length | 1,068 km | 1,778.58 km |

Land regions are approximately **24% smaller**, ocean regions **29% smaller**, with nearly unchanged land fraction. These are four-connected, map-clipped sampled areas; narrow straits and streams can fall between pixels. Rivers remain part of their continental land region. The ocean result represents multiple connected basins, not one guaranteed global ocean. V22 remains substantially more coherent than V9 in the inspected maps.

### Fifty deterministic Survival seeds

| Measurement | V21 | V22 |
|---|---:|---:|
| Collision-safe spawns | 50/50 | 50/50 |
| Cave mouth within 600 blocks | 27/50 | 50/50 |
| Median nearest cave distance | 520.4 blocks | 229.7 blocks |
| Mean coarse reachable biome types | 9.66 | 13.54 |
| Mean longest spawn-biome ray, capped at 1,500 blocks | 305.84 blocks | 197.20 blocks |
| Minimum land samples in surrounding 33×33 grid | 241 | 457 |
| Median initial spawn search | 14.0 ms | 72.7 ms |

An initial V22 fallback selected small land patches in five ocean-heavy seeds. The bounded second search and land-footprint requirements corrected those cases. Coordinates and individual results are preserved in [spawns.csv](geography-v22/spawns.csv).

Cave proximity is horizontal distance to accepted native plans, **not an exact walking distance**. Reachable-biome estimates use a 64-block terrain grid with water/relief exclusions. Dense four-block route samples and ground captures supplement it, but do not prove every route is walkable or guarantee escape from every biome within a fixed time. No literal 20-minute playthrough was automated for all 50 worlds.

### Caves, rivers and visual inspection

- All accepted entrances in the three sampled origin windows were checked against native cavities and actual generated blocks along both bore segments, including two-block head clearance, protected areas, dry routes and limited grades. Removing artificial origin land naturally removes origin-window entrances for ocean origins; counts alone are not an exposure-normalized density comparison.
- In seed 20260917's sampled 1.5 km window, accepted entrances increased **14 → 20**. The other two V22 origin windows are ocean dominated and contain no accepted openings. Spawn-relative proximity is the relevant comparison above.
- **24 actual Survival walks** through 12 entrances at 30/60 FPS reached their native junctions using ordinary forward/jump controls, with no teleportation/flying. All ended at 20 HP; the maximum recorded fall event was **0.033 HP** under the existing physics integration. A prior approach caused about 12 HP fall damage; it was rejected and the supported-floor/landing correction was implemented before finalization.
- **144 native river transects** contain generated water/ice with solid beds. Planned widths are 2.6-24 blocks; observed transects range 2-33, including intersections and adjacent ocean-mouth water. Reach endpoints descend monotonically. The graph follows a coarse geographical potential and carves valleys; it is not a global hydraulic/erosion simulation.
- Actual engine captures were inspected from elevated and ground viewpoints. [Forest](images/geography-v22/forest.png), [jungle](images/geography-v22/jungle.png), [coast](images/geography-v22/coast.png), [mountains](images/geography-v22/mountains.png), [supported cave mouth](images/geography-v22/supported-mouth.png), [river at ground level](images/geography-v22/river-ground.png), [ground exploration](images/geography-v22/spawn-ground.png), [origin flight](images/geography-v22/origin-flight.png), [cave interior](images/geography-v22/cave-interior.png). Captures were refreshed on the final build and use identical seeds/coordinates per pair, so an old ocean or mountain may become a different landscape. The interior preview includes one fixture torch for visibility, not generated lighting.

## Performance

i7-14700F / Radeon RX 9060 XT / 32 GiB RAM; Release, 1280×720, uncapped, MSAA/VSync off, SD8, weather rendering off, LOD/occlusion off. Native streaming fixture uses the same absolute coordinates, 8× flight and teleport in both generators. Complete per-phase rows: [performance.csv](geography-v22/performance.csv).

After-flight recovery phase (RD32 is the median of two runs; other rows are one run):

| RD | FPS V21 → V22 | Mean ms | p99 ms | RAM MiB | Terrain live MiB |
|---|---:|---:|---:|---:|---:|
| 8 | 5,550 → 3,319 | 0.180 → 0.301 | 0.286 → 0.378 | 271.8 → 260.2 | 91.6 → 76.6 |
| 16 | 1,685 → 1,695 | 0.594 → 0.590 | 0.716 → 0.653 | 346.2 → 318.1 | 323.5 → 276.0 |
| 32 | 510 → 554 | 1.963 → 1.806 | 2.126 → 1.997 | 631.2 → 628.2 | 1,155.9 → 1,067.8 |
| 64 | 139 → 143 | 7.206 → 7.014 | 19.421 → 8.168 | 1,655.8 → 1,698.9 | 4,602.5 → 4,484.8 |

RD8's measured mean regressed by **0.121 ms** despite fewer vertices; RD16's mean difference is negligible. RD32 improved in these samples. These are different generated scenes with identical paths, not isolated renderer speedups. Earlier runs varied, particularly at thousands of uncapped FPS. The streaming fixture does not record foreground-focus percentages. No general low-end speedup is claimed.

RD32 loaded terrain vertex count was **58.65M → 54.18M**, live buffer data about **7.6% lower**. RD8-32 queues drained within approximately 0.31 seconds after flight. RD64 remains expensive: about 4.48 GiB live terrain buffers and 1.66 GiB working RAM. Teleport tests still showed major stalls: a repeated pair reached **412 ms V21 / 488 ms V22**, and an earlier V22 retirement outlier reached **846 ms**. Both remained incomplete after the fixed 20-second teleport phase. This limitation is documented; render distance/quality were not reduced and the renderer was not rewritten to mask it.

The final RD32/RD64 runs use the cached binary. RD8/RD16 use the same final geometry before the cache-only revision; exact block-hash comparisons prove unchanged output. All source data/provenance are retained rather than mixing faster prototype numbers into this table.

### Generation cost

96 distributed chunks per version across three seeds, three repeats each:

| Mean time | V21 | V22 |
|---|---:|---:|
| Cold regional/chunk request | 3.888 ms | 5.104 ms |
| Warm repeat | 3.168 ms | 3.261 ms |

Cold requests cost approximately **31% more**, warm repeats **3% more**. Supported entrance searches account for the extra regional work. Caching exact native-column probes reduced V22's earlier cold mean **9.333 → 5.104 ms**, while all **288 V22 repeated chunk records** retained identical block payload hashes. This is a real remaining cost, not a generation speedup against V21. Continuous streaming amortizes cached plans; no physical dual-core/integrated-GPU machine was available for this pass.

## Compatibility and regression results

- **35/35 biomes** obtainable in the surveyed V22 maps; rare biomes remain naturally selected.
- **9,437,184 float values** (temperature, humidity and both macro fields) bit-identical between V21/V22 across the nine map pairs. Surface altitude cooling and weather routing passed.
- **567 baseline chunk comparisons** spanning generators 1-21, three seeds and negative/distant coordinates, matched the preserved v2.13.1 executable. Full/demo optimization smoke hashes match the same baseline.
- The underground suite passed **2,928 historical records** for generators 1-20, **182 connected layouts** (67 mineshafts / 93 dungeons / 22 ruins), deterministic loot, modified/destroyed chest persistence, finite encounter persistence, and save/load generator preservation through 21. V22 save/reload and unexplored-chunk equality passed separately.
- V22 repeat/concurrent generation and boundary probes passed. Natural blocks above Y256 were rejected by generated-volume assertions. World bounds remain -64..319 and sea level 64.
- Full/demo Survival, crafting, Recipe Book, equipment, physics/combat, command permissions and optimization regressions passed. Full weather/fluid suites passed. Each optimization suite passed eight raw/packed/indexed pixel scenes, 480 full-scene comparisons, parallel byte-identical meshes, stale-job/epoch handling, negative coordinates, exact GPU upload and resource lifetime checks.
- Weather tests were corrected to explicitly load their fixed roof/strike locations after spawn relocation; production weather/command behavior was unchanged.
- All four billboard mobs' rendering/AI/movement/spawning code and sprite assets are untouched. Terrain rendering, packed/indexed geometry, water ordering, lighting/AO algorithms, LOD and occlusion implementation are untouched.

## Files and release builds

New runtime modules: `GeographicDiversity`, `RiverNetworkV22`, `DiverseCaveEntrances`, `WorldDiversity`. `WorldDiversityValidation` and two tools provide reproducible audits/maps/benchmarks. Integration is limited to generator gates/defaults/load acceptance, cave-mask cleanup, spawn/respawn selection and fixtures. README and this evidence set document results.

- Full: `dist/VoxelFrontier-v2.13.2-Windows-Setup.exe`
- Standalone demo: `dist/VoxelFrontier-v2.13.2-Windows-Standalone.exe`
- Checksums: `dist/SHA256SUMS.txt`

The installer packages the full game; the standalone executable is the stateless demo. Both contain embedded assets and retain their full/demo persistence and permission behavior.
