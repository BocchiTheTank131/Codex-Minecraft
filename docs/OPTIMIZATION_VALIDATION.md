# v2.13.1 Optimization validation

Date: 2026-10-13. Baseline: released v2.13.0, commit `0a04ed8d664045b18de70cb0ab49ef200a4f4364`.

## Implemented

- **Lossless 28-byte terrain records**, replacing 48-byte GPU records. Full float32 world positions and UVs remain intact, including custom models and atlas padding. Normal direction, exact sunlight float bits, block light and quantized AO share an integer attribute; material and RGB tint remain unchanged. A 64-entry exact sunlight table is uploaded once when linking the shader. Integer attributes and decoding use OpenGL 3.3.
- **Mesh-local complete-attribute quad indexing**: common corners are shared only after all 48 original attribute bytes match. Triangle order, winding, provoking vertices, UV seams, lighting and tint remain exact. No generic per-vertex hash map. Meshes with at most 65,536 unique vertices use uint16 indices; larger meshes use uint32. This removes one third of duplicated quad vertices. Typical 16-bit quads use 124 bytes instead of 288: **56.94% less upload/live-buffer data**, or 20.67 bytes per original duplicated vertex including indices. The unique record itself is 28 bytes, not 20–24.
- Paired VBO/EBO support in the existing shared pages, range merging, main-thread upload and fence retirement. Water also uses shared pages and **only consecutive runs in its original sorted draw sequence** are batched. Distance rules, off-screen sort candidates and equal-distance tie behavior remain unchanged. Missing base-vertex support falls back to expanded 28-byte arrays.
- Exact stationary visibility-list reuse keyed by camera, view/projection, center, radius and mesh revision. Edits, uploads, unloading and regeneration invalidate it. Only touched opaque pages clear their draw lists.
- **Optional asynchronous far-detail LOD**, levels 0–2: level 1 omits distant grass/ferns/dead bushes; level 2 also omits tiny flowers/mushrooms. Solid terrain, canopy geometry, structures, torches, doors, cobwebs, crops, bamboo, water/glass/ice and all physics remain full resolution. It uses existing mesh workers, immutable snapshots, revisions, near-first priority, 32-block hysteresis and bounded 64-chunk scheduling. Off restores all requests to full detail. One GPU variant per chunk is resident; duplicate full/distant caches are not retained.
- **Optional conservative occlusion**: at most 64 coarse groups and 8 AABB queries per frame, after real opaque depth. Padded bounds, near-plane exclusion and exact camera/viewport/MSAA/world-revision proofs; expired/unknown/moving/edited visibility draws normally. Query results and both GPU timestamp endpoints are read only after availability checks. Auto compares GPU probes and CPU overhead, backs off for 240 frames, and defaults to ordinary rendering if timing support is unavailable. Water is never filtered by occlusion.
- Memory presets control allocation slack and idle-page reserves. Low: zero growth slack / no idle reserve; Auto and Balanced: 6.25% / 16 MiB; High: 12.5% / 64 MiB. Soft targets are 512 MiB / 2 GiB / 4 GiB; pressure removes idle reserves, with at most one excess page retired per frame. **These are soft allocation targets, not hard VRAM limits. Visible meshes are retained when they exceed the target.**
- Video settings and persisted clamps for Terrain LOD Off/Low/Medium/High, Occlusion Off/Auto/On and Terrain Memory Target Auto/Low/Balanced/High. Both LOD and occlusion default Off. F3 reports drawn vertices/triangles, terrain calls, levels, culling/query counters, buffer used/capacity/free/retired/target, queues, workers, throughput, generation averages, mesh/upload and terrain submission timings. Diagnostics fit the screen and memory scans run only with diagnostics or explicit fixtures enabled.

## Phases and measurement method

The original full/demo binaries and source were preserved **before implementation** in `build/v2131-validation/`. Existing Release streaming fixtures ran at RD8/16/32/64 into `baseline-rd*`; the packed-only phase ran the same workloads into `packed-rd*`. Separate packed and LOD/occlusion snapshots remain available. Default final comparisons use LOD Off and occlusion Off: no graphics reduction is included in the main gains.

Original-engine foreground instrumentation adds only opt-in fixture hooks. Its original mesher, 48-byte renderer, storage and gameplay stay unchanged. Both variants run the same foreground fixture with seed 123456789, Generator 21, 1280×720, default FOV 74, SD8, MSAA Off, VSync Off, uncapped FPS, effect quality 2, entity distance 12. Scenes warm until queues settle or the stated RD-dependent timeout, then measure 3 seconds each stationary, rotating and editing. The gameplay scene runs Survival, Diamond armor/shield, the existing four billboards and Medium rain; simulation, physics, fluids, AI and audio remain active. It is scripted gameplay, not an unrestricted manual combat session.

GPU intervals end before buffer presentation and use an available-results-only ring. Whole-frame and render-call wall times still include normal presentation. Earlier `foreground-*`/`confirm-*` experiments included presentation inside their GPU interval and are **not used for final GPU claims**. Unfocused rows are excluded. The final table uses only 99%+ focused and visible rows from `gpu-*`; all reported final runs were 100% focused/visible. No compilers or other fixtures ran during measurements.

Hardware: i7-14700F (28 logical threads), Radeon RX 9060 XT, 32 GiB system RAM. Absolute uncapped FPS is specific to this hardware/settings. Low-end hardware and a separate integrated GPU were not physically tested; 1/2/4/8/16/28-core worker plans were regression tested.

## Foreground before / after

Stationary medians; units are milliseconds except FPS and calls. Each listed run also records rotation/editing in the raw data. Runs are short and these values do not guarantee worst-case long-session performance.

| RD / scene | Runs per variant | FPS before → after | Mean ms | p95 ms | p99 ms | Worst ms | GPU ms | Draw calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 / plains | 1/1 | 4349 → 5418 | 0.230 → 0.185 | 0.275 → 0.222 | 0.467 → 0.474 | 2.795 → 2.765 | 0.149 → 0.103 | 15 → 7 |
| 8 / forest | 2/2 | 2884 → 4657 | 0.347 → 0.215 | 0.403 → 0.258 | 0.473 → 0.456 | 2.703 → 2.725 | 0.265 → 0.134 | 13 → 6 |
| 8 / mountains | 2/2 | 2325 → 4161 | 0.430 → 0.240 | 0.491 → 0.286 | 0.550 → 0.469 | 2.147 → 2.195 | 0.347 → 0.161 | 19 → 8 |
| 8 / cave | 2/2 | 1912 → 3223 | 0.523 → 0.310 | 0.588 → 0.362 | 0.643 → 0.442 | 1.515 → 1.696 | 0.441 → 0.232 | 68 → 33 |
| 8 / ocean | 2/2 | 4773 → 5321 | 0.210 → 0.188 | 0.254 → 0.230 | 0.473 → 0.475 | 1.767 → 1.645 | 0.128 → 0.105 | 68 → 14 |
| 8 / structures | 1/1 | 2265 → 3932 | 0.442 → 0.254 | 0.510 → 0.305 | 0.588 → 0.475 | 1.509 → 1.887 | 0.357 → 0.177 | 83 → 37 |
| 8 / gameplay | 2/2 | 3917 → 4754 | 0.255 → 0.210 | 0.325 → 0.271 | 0.609 → 0.612 | 3.029 → 3.113 | 0.166 → 0.120 | 16 → 6 |
| 16 / forest | 2/2 | 1185 → 2166 | 0.844 → 0.462 | 0.924 → 0.516 | 0.997 → 0.575 | 1.913 → 1.561 | 0.754 → 0.383 | 48 → 22 |
| 16 / cave | 2/2 | 684 → 1357 | 1.462 → 0.737 | 1.565 → 0.798 | 1.623 → 0.849 | 2.473 → 1.815 | 1.349 → 0.658 | 294 → 140 |
| 16 / ocean | 2/2 | 1954 → 3335 | 0.512 → 0.300 | 0.578 → 0.351 | 0.647 → 0.490 | 1.926 → 1.913 | 0.428 → 0.220 | 265 → 98 |
| 16 / gameplay | 2/2 | 1520 → 2689 | 0.658 → 0.372 | 0.735 → 0.423 | 0.781 → 0.522 | 1.689 → 1.542 | 0.571 → 0.293 | 82 → 44 |
| 32 / forest | 2/2 | 364 → 742 | 2.746 → 1.349 | 2.843 → 1.434 | 2.914 → 1.497 | 3.328 → 2.189 | 2.644 → 1.250 | 296 → 174 |
| 32 / cave | 2/2 | 240 → 498 | 4.171 → 2.006 | 4.260 → 2.086 | 4.343 → 2.162 | 4.715 → 2.975 | 4.069 → 1.906 | 838 → 441 |
| 32 / ocean | 2/2 | 605 → 1173 | 1.653 → 0.853 | 1.739 → 0.913 | 1.810 → 0.969 | 2.375 → 1.949 | 1.552 → 0.766 | 1054 → 573 |
| 32 / gameplay | 2/2 | 438 → 879 | 2.284 → 1.138 | 2.377 → 1.200 | 2.450 → 1.259 | 3.153 → 2.315 | 2.182 → 1.054 | 517 → 278 |
| 64 / mountains † | 1/1 | 24 → 166 | 41.968 → 6.041 | 116.585 → 6.133 | 119.830 → 6.355 | 142.759 → 7.153 | 33.206 → 5.940 | 1481 → 1237 |

† **RD64 is a memory-pressure stress test, not a matched-workload speedup.** After the bounded warmup the baseline was still streaming: 10,570 chunks / 19.13 million drawn triangles versus 12,853 / 24.05 million final. Baseline live buffers/capacity were 7.88/9.06 GiB; final 4.25/4.78 GiB, with more completed geometry. Its large FPS change must not be described as a general 7× rendering improvement. RD8–32 forest pairs have matching loaded counts (197/797/3,209) and triangle coverage apart from small simulation/edit changes.

Forest mean frame time decreased approximately **38%, 45%, 51%** at RD8/16/32. GPU intervals decreased approximately **50%, 49%, 53%**. Packing plus indexing is the biggest measurable gain. RD8 p99/worst timing did not materially improve; sub-millisecond tails and occasional OS/driver stalls remain.

## Memory and CPU costs

| RD / forest | Live VBO+EBO MiB before → after | Capacity MiB | Working set MiB | Private commit MiB |
|---|---:|---:|---:|---:|
| 8 | 107.5 → 46.3 | 127.9 → 56.0 | 284.7 → 292.2 | 598.8 → 483.1 |
| 16 | 439.0 → 189.0 | 504.4 → 216.0 | 350.4 → 359.2 | 1113.4 → 741.2 |
| 32 | 2066.2 → 889.6 | 2370.3 → 1000.0 | 601.6 → 599.6 | 4605.0 → 1879.8 |

Live terrain data fell **56.94%** at matching RD8–32 forest geometry. At RD32 allocated capacity fell about **57.7%**. The RD32 final live split is approximately 803.5 MiB VBO + 86.1 MiB EBO. Working-set RAM changes at RD8–32 were small (slightly higher at RD8/16); private commit fell substantially, including driver backing allocations. Buffer capacity here counts the game's terrain VBOs/EBOs only; it is not Task Manager's total physical VRAM usage.

Fixed snapshots, 25 timed repetitions after 5 warmups:

| Chunk | Raw mesh ms | Pack only ms | Pack + index ms | Original → unique opaque vertices |
|---|---:|---:|---:|---:|
| 0,0 | 6.34 | 0.311 | 0.236 | 18,858 → 12,572 |
| -1,-1 | 6.21 | 0.423 | 0.318 | 25,848 → 17,232 |
| 1,1 | 6.73 | 0.430 | 0.331 | 25,452 → 16,968 |

Packing/indexing adds roughly **3.7–5.1%** to these raw build times; it is faster than packing all duplicated vertices. Worker scratch and returned payload pools are bounded. These numbers are fixed snapshots, not an average of every biome. Generator 21 across 27 identical chunk samples averaged 3.419 ms baseline / 3.467 ms final (+1.4%); that is timer/workload noise, not a claimed generation improvement.

## LOD and occlusion results

RD32 forest Off → High far-detail LOD: 1.349 → 1.292 ms frame, 1.250 → 1.191 ms GPU; 4.933 → 4.696 million drawn triangles. Cave: 2.006 → 1.947 ms frame, 1.906 → 1.857 ms GPU. Additional gain is approximately **3–4%**, with optional far decoration omissions; no terrain silhouette or canopy simplification is claimed. Near geometry and all transparent geometry remain exact. Level 3 is absent and its diagnostic count is zero.

Occlusion Auto produced **no reliable aggregate speedup** in these live forest/cave views: forest 1.349 → 1.370 ms, cave 2.006 → 2.007 ms. Auto backed off; end-of-phase counters were zero. Forced On skipped 2,936 hidden chunk draws across the deterministic correctness capture sequence, with identical pixels. This validates conservative correctness, not a proven general FPS gain. Occlusion remains Off by default and Auto/On are optional. Moving cameras or frequent terrain updates deliberately fall back to visible.

## Streaming and frame pacing

The existing 20 s load / 8 s 8× flight / 12 s recovery / 20 s teleport fixtures ran before and after at every RD. RD32 flight mean 5.022 → 3.433 ms, recovery 5.249 → 2.814 ms, teleport 4.885 → 2.971 ms. Recovery queues drained in 0.225 / 0.248 seconds; no throughput speedup is claimed from that small difference. Geometry at phase ends can differ during streaming, so these are workload/recovery results, not fixed-geometry GPU comparisons.

**Worst teleport stalls are not solved.** One pair had 25.5 ms baseline / 59.2 ms final worst frame, with retirement-stage peaks 19.4 / 27.3 ms. Average and p99 improved, but this outlier regressed. A separate 30-second RD32 flight plus 30-second recovery drained in 0.216 seconds; teleport worst 33.3 ms and retirement peak 14.1 ms. It does not establish a consistent worst-frame reduction. Individual driver retirement/deallocation operations remain indivisible; further investigation is needed before claiming spike-free streaming. The extended final run had no stuck dirty/upload/light queues after recovery or teleport.

## Exact correctness

- **2,928 historical chunk hashes, generators 1–20**, matched the preserved fixture; CSV SHA-256 `a53f68e367c41d5a18e6d9931e8e030810210c35f944ff19f12d6e527c5a68cf`.
- **567 additional snapshots across generators 1–21 and three seeds** matched the original Release engine byte-for-byte, full and demo. Historical generator save/load and unexplored-chunk equality passed for 1–21; modified/looted chests and equipment remain compatible.
- Complete 48-byte attribute reconstruction passed CPU packing/index tests (including arbitrary UV padding, negative positions, signed-zero normals, attribute discontinuities and >65,536-vertex fallback). GPU VBO/EBO readback re-expands to the exact original mesh hash.
- Eight legacy/packed/indexed shader scenes were pixel-identical across sunlight/night, wet weather and water states. Six actual terrain captures match v2.13.0 pixel-for-pixel, full and demo.
- **480 full-scene comparisons per variant** passed with occlusion/ordered water batching, camera motion, remeshed edits, regeneration, viewport and 4× MSAA changes. 432 frames issued visibility queries, at most 8 each, and 2,936 hidden chunk draws were omitted. No OpenGL errors.
- Parallel generation/cache eviction, eight byte-identical parallel meshes, negative coordinates, epoch/revision cancellation, teleport recovery, bounds, page merging and paired resource cleanup passed. LOD retained opaque attributes/order and transparent arrays exactly; hysteresis and Off restoration passed.
- Full and demo passed Survival/equipment/crafting/Recipe Book/physics/fluids/commands/weather/patch272/menu/UI fixtures. The full Spectator save/reload fixture passed. **Demo Spectator fixture is not applicable:** it unconditionally requires saves, and fails identically in v2.13.0 and v2.13.1; supported demo stateless and command-denial tests pass. Restrictions were not weakened to satisfy that fixture.
- `Player.cpp`, `Survival.cpp`, equipment, weather, sound, item/block definitions, generation modules and the four sprite assets are unchanged. Actor animation/AI/physics and spawning code were not rewritten.

Captures: [forest](images/optimization-forest.png), [cave](images/optimization-cave.png), [water](images/optimization-ocean.png), [video options](images/optimization-settings.png).

## Rejected / deferred / incomplete

- A 20–24-byte unique record through half/fixed-point UV/position quantization was rejected: exact custom positions, padding and distant coordinates could not be guaranteed. The lossless aligned format is 28 bytes; indexing brings typical effective storage below 24 bytes per original duplicated vertex.
- Removing same-species internal leaf faces was rejected as redundant: the existing mesher already removes them. Trees/canopies remain unchanged.
- **Full terrain-surface simplification and a four-level horizon proxy are not implemented.** Current LOD is conservative far decoration detail only. Seam-safe surface reduction needs separate validation; there is no heightmap replacement that hides caves, rivers or overhangs.
- No hierarchical depth renderer, aggressive moving-camera occlusion, indexed indirect draw, or new GL baseline. The optional query path has no demonstrated general gain in the tested views.
- No hard residency eviction or live-page compaction: allocation slack/reserve improvements and merging were retained; copying live pages was not justified against frame-spike/resource-lifetime risk. Soft targets can be exceeded by live geometry, especially RD64.
- Chunk palette/RLE, vertical edit sections, SIMD noise, more workers, entity instancing and async lighting/saving were not adopted. Existing one-byte blocks, packed light, trimmed upper-air storage, cached/interpolated noise, greedy meshes, actor batching and adaptive workers already work. Meshing/generation were not the dominant steady-view bottleneck here. These techniques were not presented as tested gains.
- Local CMake/MSVC `CheckIPOSupported` passed; the toolchain supplies `/GL` and `/LTCG`. LTO/PGO are not enabled and no speedup is claimed: full-engine LTO timing/hash validation and representative PGO training were not performed. Speculative compiler changes were not mixed into the proven rendering gains.
- Multi-hour leak testing, physical integrated-GPU testing, strict VRAM caps and elimination of every worst-frame stall remain unverified/incomplete.

## Reproduction and evidence

Raw final foreground rows: [foreground.csv](optimization/foreground.csv); sequential streaming rows: [streaming.csv](optimization/streaming.csv). Build-side `gpu-*`, `stream-*`, `extended-rd32`, `audit`, `verified-pixels-*`, and `package-pixels-*` contain the individual CSVs, hashes, captures and logs. Initial experiments remain archived rather than being mixed into final results.

`tools/run_render_optimization_benchmarks.ps1` runs paired common foreground fixtures; its baseline argument must point to the **instrumented original engine**, not the uninstrumented old binary. In this workspace that is `build/v2131-validation/baseline-instrumented/Release/VoxelFrontier.exe`. The unmodified original binary can run `--streaming-benchmark=<directory> --streaming-rd=32 --streaming-flight-seconds=8 --streaming-recovery-seconds=12 --seed=123456789`. Current correctness fixture: `--optimization-smoke=<directory> --seed=123456789`. Use fresh isolated `VOXEL_FRONTIER_DATA_DIR` profiles and keep benchmark windows focused. Do not run compilers/other fixtures concurrently.

## Changed files and local builds

New modules: `include/TerrainVertex.h`, `include/TerrainLod.h`, `include/TerrainOcclusion.h`; `src/TerrainVertex.cpp`, `src/TerrainLod.cpp`, `src/TerrainOcclusion.cpp`, `src/TerrainValidation.cpp`, `src/OptimizationBenchmark.cpp`, `src/OptimizationValidation.cpp`.

Integration: `include/World.h`, `include/Game.h`, `include/Renderer.h`, `include/Settings.h`, `include/ChunkStreaming.h`, `include/MenuLayout.h`; `src/World.cpp`, `src/WorldStreaming.cpp`, `src/WorldAesthetics.cpp`, `src/WorldExplorationValidation.cpp`, `src/Game.cpp`, `src/Renderer.cpp`, `src/Settings.cpp`, `src/ContentValidation.cpp`; `CMakeLists.txt`, README, this report, benchmark utility and evidence CSV/capture files.

Both final Release variants and the installer compiled, with current 2.13.1 metadata. Full/demo binaries were tested from isolated data profiles using embedded assets (100 OGG effects + 14 music tracks). Final packaged full/demo optimization smoke checks passed. SHA-256 sums match `dist/SHA256SUMS.txt`. The installer was compiled and checked, but not installed over the user's existing game.

- Full game: `build/installed-release/Release/VoxelFrontier.exe`.
- Standalone demo: `dist/VoxelFrontier-v2.13.1-Windows-Standalone.exe`.
- Full installer: `dist/VoxelFrontier-v2.13.1-Windows-Setup.exe`.

Release packages include the full-game installer, standalone demo, and SHA-256 checksums.
