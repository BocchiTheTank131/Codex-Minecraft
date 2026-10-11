# Generator V12 and streaming validation — v2.11.2-beta

This update is prepared as the approved `v2.11.2-beta` GitHub prerelease. The beta suffix
changes release metadata, not the tested terrain or streaming behavior. Remaining
performance limitations are documented below.

## Scope and bottlenecks

The starting release is v2.11.1, commit `81b2c693a429e8121982c76c1cb2efc31ce9fc06`.
The original pipeline had up to three generation workers and one mesh worker. In the
RD64 fixture, generation finished while 10,634 chunks remained dirty after warmup.
The flight phase ended with 9,987 dirty chunks and only 230 rendered chunks. Increasing
generation alone would not solve this mesh backlog.

The new scheduler has independent hardware-aware generation and mesh pools, persistent
sleeping workers, bounded admissions, near/forward priorities and revision/epoch rejection.
An immutable halo belongs to one mesh job; each worker owns its greedy scratch. Required
neighbor arrivals are coalesced before background meshes are submitted. Edited chunks
remain high priority. OpenGL calls stay on the main thread.

On the test CPU (28 logical processors), Auto allows six generation/four mesh workers,
Balanced four/two, High eight/four, and Low one/one. Smaller hardware receives fewer workers;
one-to-three logical processors use one worker per stage. Auto reduces generation pressure
while meshes lag and reduces active mesh workers when little work remains. The maximum
pool is created once; changing quality does not create a thread per chunk.

GPU upload, integration and snapshot stages use soft 0.5–2 ms budgets. One indivisible
operation can exceed a budget. Chunk load and unload radii retain their original meaning.
Normal gameplay measures busy work before presentation, excluding VSync and FPS-limit
waiting from this budget. F3 frame time and simulation still use the real frame interval.
CPU chunk payloads and GPU ranges are detached immediately on unload. GPU ranges are
released in small main-thread batches after a nonblocking GPU fence poll; detached CPU arrays are freed by the existing
generation workers outside the queue lock. Cleanup takes priority above a 96-payload
watermark, avoiding indefinite memory debt during long flights. No new cleanup thread
or off-thread OpenGL calls are introduced.

The existing terrain page allocator now reuses a bounded idle reserve instead of deleting
every newly empty GPU page during teleports. Once settled, the reserve is at most one
8 MiB page per planned mesh worker (8–32 MiB). During active streaming, the idle-page
reserve targets 128 MiB; excess pages are trimmed one per frame, so transient reserve
size can exceed that target after a teleport. Retirement fences
prevent reuse of ranges still referenced by submitted draws. This deliberately trades a
small GPU reserve for fewer allocation/deallocation spikes; it does not hide dirty jobs.

A structure-cache ownership bug was found during the concurrency review: a reference
outlived the temporary shared owner returned by the cache lookup. Another worker could
evict that entry during inspection. The caller now retains its shared owner while copying
the plan. The regression churns more than the 128-plan cache capacity across eight workers
and compares 128 generated chunk payloads with serial reference hashes.

## Generator changes and compatibility

Only generator 12 changes terrain/vegetation. Continentalness, macroclimate and the 35-biome
catalog remain. Medium-scale local habitat/humidity fields select compatible subregions;
an inland relief mask adds rolling hills while fading away from rivers and shorelines.
The V11 density lattice, cave families, ore distributions and regional aquifers remain.

Mushrooms use deterministic cluster anchors. A synthetic 128×128-block habitat isolates
spacing from terrain and tree changes:

| Habitat | V11 mushrooms | V12 mushrooms | Columns |
|---|---:|---:|---:|
| Swamp | 5,294 | 8 | 16,384 |
| Dark Forest | 3,746 | 39 | 16,384 |
| Mushroom Fields | 4,944 | 980 | 16,384 |

These are controlled flat habitats, not a promise of the same counts in real terrain.
Final ordinary ground-decoration totals also fall from 5,294 to **1,368** in Swamp and
3,746 to **1,001** in Dark Forest (about 74% and 73% fewer decoration blocks). Failed
mushroom candidates are not all replaced with grass/ferns; three quarters remain open.
Tree anchors reject crowded lower-priority candidates using seed/world coordinates.

Surface entrances use bounded regional candidates, reject wet/low terrain and structure
reservations, and require a dry native density cavity as their endpoint. They carve joined
passages, occasional wider mouths and sinkholes. Tests sample the actual generated blocks
along each passage, checking two-block-high air clearance and the native cave connection.
Sparse cavity queries reproduce the existing lattice interpolation exactly.

Three seeds (`20260917`, `123456789`, `42`) are surveyed. All 35 biomes remain available.
Maps cover 4,096 and 30,720 blocks at 512×512 samples, comparing V11/V12 biomes, temperature,
humidity, continentalness and terrain. Route tests use 192 paths, each approximately 2.1 km,
including oceans; land-only summaries require at least 80% land.

On the 73 routes with at least 80% land in both versions, the average number of distinct
biomes is 5.64 in V11 and 5.74 in V12. Boundary crossings fall from 15.21 to 8.97; the
average elevation range rises from 89.7 to 92.0 blocks. These are modest variety/relief
gains, with larger coherent local patches. Two V12 routes still remain entirely within
one biome (Snowy Peaks and cold/dry Windswept Hills). The generator does not force a
climatically inappropriate forest into those routes to improve the count.

Connected entrance plans found around the approximately 1.5 km survey, including the
entrance-planning halo, are 9 / 1 / 6 for the three seeds. This is a small survey, not a
world-wide density estimate. Each sampled route into a plan's native cave is checked
against generated blocks rather than relying only on its analytic carve shape.

F3/F4 now distinguish surface temperature from camera-altitude temperature. A high camera
can correctly be cold above warm terrain. Weather already samples exposed surface elevation;
the update does not substitute camera altitude into precipitation or freezing mechanics.

## Correctness checks

- Eight concurrent builds of one immutable snapshot have byte-identical opaque and
  transparent vertex output, including attributes and ordering.
- Concurrent generation remains identical during structure-cache eviction.
- Sunlight from the contiguous bulk traversal matches the original column traversal.
- Worker plans are checked at 1/2/4/8/16/28 logical processors and every quality level.
- Negative-coordinate travel, repeated teleports, in-flight world reset, quality changes,
  edits and epoch/revision cancellation recover bounded queues.
- GPU readback after cancellation/edits matches a fresh reference mesh byte-for-byte.
- Sparse density samples match the cached grid at negative coordinates and chunk borders.
- Negative/upper Y edits, lighting removal, meshes, raycasts, collision, lava, roofs,
  structure support/loot and generator-specific save roundtrips are covered.
- An untouched world save produced by the released v2.11.1 executable loads and resaves
  with generator 11, its position and geography preserved.
- V11 generation hashes are compared with the prior release's saved baseline.

The final packaged executable reproduces all **432 V11 generation hash rows** with zero
mismatches. The final legacy audit reproduces all **480 generator 1–10 hash rows**, also
with zero mismatches. In the same packaged audit, 432 timed builds per version average
**2.719 ms for V11 and 2.758 ms for V12** (about 1.4% slower). The prior packaged repeat
measured 2.838 / 2.917 ms (about 3% slower). This small CPU difference is
not presented as a generation-speed improvement; streaming improvements come primarily
from concurrency, scheduling, mesh coalescing and cleanup.

Two existing validation assumptions required correction. The old four-structure-region
survey can find no structures around a continental ocean spawn; the released v2.11.1
executable reproduces that failure. Continental smoke tests now use the density audit's
48-region bound. The v2.7.2 legacy terrain fixture also indexed old-height chunks through
the current negative-Y world. It now explicitly creates generators 7/8 and indexes their
own storage. Neither correction changes structure placement or legacy terrain.

## Release builds and packaged testing

Both final MSVC Release variants build successfully with embedded runtime assets:

- `VoxelFrontier-v2.11.2-beta-Windows-Standalone.exe`: compile-time demo mode enabled.
- `VoxelFrontier-v2.11.2-beta-Windows-Setup.exe`: full game with demo mode disabled.

The final binaries were tested from an isolated directory outside the repository. Demo
menu/weather fixtures load embedded assets and audio, and create no save/config/seed
files or saved-world directories. Installer testing covers clean installation, launch,
upgrade from v2.11.1, repair, shortcuts and uninstall. Old world files are unchanged by
installation; old worlds retain generator 11 and new worlds use generator 12. Uninstall
preserves worlds/settings. Installed payload SHA256 matches the final full Release build.
The user's original local data and shortcut were preserved throughout the test.

Final prerelease artifact hashes are supplied in the attached `SHA256SUMS.txt`,
regenerated from the actual beta binaries. Package checks verify the installed payload
against its full Release build and verify both manifest entries.
Uncapped final-vegetation timings used build
`b7b9508f4320b29f27b744d1a847f7f934fa42c5447cf3268b565ab84c56b71d`;
the subsequent capped-game timing fix only changes the normal loop's budget input.
Final packaged hashes reproduce both V11 and V12 payloads from that timed build.

Final Release fixtures pass for menu/multiple worlds, weather/all 19 weather sounds,
commands/crafting, Survival/combat/52 mob terrain cases, lighting/AO/fluids, safe respawn,
tooltips, Spectator, legacy geography and the new exploration/concurrency audit. Actual
gameplay captures include forest/dark-forest vegetation, swamp climate at ground and high
camera altitude, snowy mountain entrances, a dry cave interior and an exposed ravine.
Final forest, dark-forest and swamp captures run the normal game loop at a 60 FPS cap;
all requested RD16 chunks finish and queues are empty in those captures. This is a
functional capped-loop check, not a capped-RD64 performance benchmark.
The existing `Survival.cpp` int-to-float compiler warning remains; no new compiler errors
were introduced. Test profiles, maps, captures and build intermediates remain ignored.

## Reproduction

Use an isolated `VOXEL_FRONTIER_DATA_DIR`; do not run development tests on real saves.

```text
VoxelFrontier.exe --exploration-audit=<absolute-directory> --seed=123456789 --generator=12
VoxelFrontier.exe --streaming-benchmark=<absolute-directory> --seed=123456789 --generator=11
VoxelFrontier.exe --streaming-benchmark=<absolute-directory> --seed=123456789 --generator=12
VoxelFrontier.exe --streaming-benchmark=<absolute-directory> --generator=12 --streaming-rd=64 --streaming-flight-seconds=90
```

The benchmark fixes 1280×720, time 35, clear weather, simulation distance 8, F3 off,
VSync/MSAA off. Each distance has 20 seconds warmup, a flight at `(68,0,-17)` blocks/sec,
40 seconds stationary recovery, then a 20-second teleport phase. The extended fixture
uses 90 seconds flight. Tests run serially, without simultaneous builds or previews.
This isolates streaming/rendering; the benchmark directly drives the camera rather than
running a crowded Survival simulation.

CSV records mean FPS/frame time, p99/worst frame, process CPU/working-set RAM, loaded/rendered
chunks, dirty/generation/mesh/upload queues, active workers, throughput and CPU stage times.
OpenGL upload timings measure API-call CPU time, not hardware GPU timestamp queries.
Unfinished baseline meshes mean some before/after frames draw different amounts of terrain;
raw FPS alone is not a fair measure of streaming progress.

`--streaming-recovery-seconds=120` extends stationary recovery without changing gameplay.
`streaming-samples.csv` records queues and working-set/private memory every five seconds
to distinguish accumulating cleanup debt from a denser destination. The V12 column
below is the final vegetation repeat with this trace enabled. The V11 isolation matrix
predates the trace; its streaming implementation is unchanged. The final normal-game
idle-time accounting fix does not change the uncapped benchmark's direct timing input.

## Standard 8× flight measurements

Baseline is the untouched v2.11.1 pipeline. The same-generator column uses V11 terrain in
the new pipeline, isolating scheduling from V12 geography. All figures are one complete
serial matrix, not a claim of statistical significance from many repeated trials.

| RD | Baseline FPS | New pipeline/V11 FPS | Baseline mean / p99 ms | New pipeline/V11 mean / p99 ms | V12 mean / p99 ms |
|---:|---:|---:|---:|---:|---:|
| 8 | 7,177 | 6,752 | 0.139 / 0.365 | 0.148 / 0.303 | 0.147 / 0.292 |
| 16 | 1,421 | 1,249 | 0.704 / 1.993 | 0.801 / 3.708 | 0.783 / 3.734 |
| 32 | 710 | 357 | 1.409 / 3.256 | 2.801 / 6.402 | 2.746 / 6.414 |
| 64 | 117 | 131 | 8.534 / 62.369 | 7.617 / 23.051 | 7.674 / 23.184 |

| RD | Baseline → new V11 dirty chunks | Baseline → new V11 rendered chunks | Baseline → new V11 RAM MiB | Baseline → new V11 CPU % | New V11 generation / meshes per second |
|---:|---:|---:|---:|---:|---:|
| 8 | 16 → 25 | 35 → 33 | 184 → 166 | 8.87 → 10.88 | 72 / 151 |
| 16 | 63 → 32 | 229 → 249 | 258 → 246 | 7.11 → 8.78 | 141 / 266 |
| 32 | 2,194 → 63 | 236 → 1,035 | 509 → 561 | 8.71 → 14.98 | 277 / 510 |
| 64 | 9,987 → 294 | 230 → 2,582 | 1,290 → 1,388 | 11.11 → 16.93 | 383 / 442 |

At RD64, mean flight time improves about 11% and p99 about 63%, while substantially more
terrain is complete. RD32 is **not an FPS improvement**: it renders about 4.4× as many chunks
by the end of flight and consumes more CPU/GPU time. RD8 overhead is small; RD16 p99 is
worse. These regressions are retained in the report rather than hidden by the RD64 result.

Loaded chunks at flight end are 241 → 246 (RD8), 877 → 882 (RD16), 3,361 → 3,361
(RD32), and 11,742 → 9,726 (RD64) in the same-generator comparison. More complete
nearby meshes do not mean generation has already filled the entire requested radius.
Admission remains bounded during movement; the full radius finishes during recovery.

After stopping at RD64, the new pipeline settles all stage/lighting/retirement queues in
7.91 seconds on V11 and 7.37 seconds on final V12. The old pipeline still has 3,299 dirty chunks
after 40 seconds. Settled V11 RAM is 1,581 MiB versus baseline 1,495 MiB (about 6% higher),
with 4,096 rendered chunks versus 2,911. Extra worker scratch/output storage and the
bounded GPU page reserve have a memory cost.

Baseline RD64 teleport's worst frame is 3,463 ms. Implementation repeats produce
191 / 230 / 396 ms worst frames; the final V12 standard matrix reaches 529 ms, with
511 ms in GPU retirement. These are different invocations, not a guaranteed maximum. This is a
large reduction, **not hitch-free teleportation**. Driver/API retirement still exceeds
the soft budget on occasional calls. Standard flight worst frames are 32.2 ms (V11) and
26.3 ms (final V12), versus 97.7 ms baseline.

Final High (eight/four workers) gives 7.728 ms mean and 23.074 ms p99 at RD64, essentially
the same as Auto, and settles in 8.18 seconds. Extra generation workers do not materially
improve this workload. An earlier Low (one/one) RD8 repeat gives 0.139 ms mean / 0.397 ms
p99. Final Low at RD64 gives 3.213 ms mean / 11.169 ms p99 and leaves 4,210 dirty chunks
after flight; its higher FPS partly reflects unfinished terrain. Low preserves
distance/quality but intentionally takes longer to finish work.

### Extended flight and memory convergence

The final repeat includes the fallback ground-decoration reduction. The same-pipeline
V11 control is unaffected by that V12-only change.

A 90-second RD64 flight (about 6.3 km including lateral movement) reaches denser terrain
than the short matrix. Final V12 flight averages 7.168 ms, p99 19.287 ms, worst
40.340 ms, and 22.26% CPU across all 28 logical processors. It ends with 287 dirty chunks.
The original 40-second recovery window was insufficient for this destination. With a
120-second window, **all queues reach zero at 84.99 seconds**, with 13,018 loaded and
4,137 rendered chunks. Working set remains exactly 5,425 MiB in the five-second samples
from 85 through 115 seconds; private commit likewise plateaus at 20,209 MiB.

This is **not a low-memory RD64 workload**. Private commit is not resident RAM or measured
VRAM; its allocation ownership has not been fully profiled. A stable settled plateau is
evidence against an indefinitely growing cleanup backlog in this run, not proof that
all memory is optimally used. Recovery averages 28.10 ms, p99 57.22 ms, with 25.84 ms in
render submission/presentation. Completing more dense terrain exposes a substantial
rendering/memory cost. No terrain, distance or graphics-quality reduction is used to
hide it. Physical low-end GPU/RAM verification is still required.

The same new pipeline and 90-second route using **V11 terrain** also has a large settled
footprint: 4,170 MiB working set / 19,016 MiB private commit, with queues settling at
47.45 seconds. Its recovery mean/p99 are 20.465 / 31.086 ms, versus V12's
28.097 / 57.219 ms. Final V12 is therefore **about 37% slower in mean recovery frame time,
30% higher in working set, and 6.3% higher in private commit in this dense destination**.
Both plateau after settling. This is a genuine scene-dependent regression, not dismissed
as timer noise. Allocation ownership and GPU execution have not been fully profiled;
the CPU render timing includes presentation. Further RD64 GPU/memory work would need
separate investigation rather than changing quality or silently reducing distance.

Before thinning fallback plants, a prior V12 repeat settled at 74.56 seconds / 5,473 MiB
and averaged 23.96 ms during recovery. The final repeat uses slightly less memory but
does not demonstrate a frame-time improvement. Dense-scene timing varies materially;
these individual trials are evidence of a remaining RD64 problem, not a statistical
claim about the vegetation change. No hardware GPU timestamps or allocation attribution
were collected, so the rendering/driver-memory explanation remains incomplete.

The final one/one-worker Low repeat clears every queue in **41.02 seconds** after the
short flight, then stays near **1,512 MiB working set** through 120-second recovery.
Earlier repeats cleared in 39.13 seconds or still had 951 dirty chunks at 40 seconds.
This variation is why the report includes convergence traces rather than promising a
fixed recovery time.

## Files and validation artifacts

- `ChunkStreaming` and `WorldStreaming`: worker plans, queue ownership, priorities,
  revisions, stage budgets, diagnostics and retired-resource handling.
- `TerrainExploration`, `WorldExploration`, `WorldDensity`, `BiomeVegetation` and
  `TerrainDensity`: versioned habitats/relief, clustering, connected entrance plans and
  exact sparse density queries.
- `Game`, `Settings`, `MenuLayout`, `UIManager`, `Renderer`: streaming quality,
  surface/camera climate labels and fixture integration.
- `Structure`: retain shared ownership during concurrent cache eviction.
- `StreamingValidation`, `WorldExplorationValidation`, `WorldDensityValidation`,
  `WorldValidation` and preview tooling: regression fixtures and export support.
- `CMakeLists.txt` and `README.md`: v2.11.2-beta metadata, modules and usage documentation.

Ignored local artifacts include `build/v12-baseline`, `build/v12-verified-v11`,
`build/v12-verified-v12`, the Low/High matrix directories, `build/v12-final-extended`,
`build/v12-final-low-recovery`, `build/v12-final-v11-extended`,
`build/v12-final-matrix`, `build/v12-final-dense`, `build/v12-final-low`, `build/v12-final-high`,
`build/v12-release-regressions`, `build/v12-gameplay`, and `build/v12-final-gameplay`.
The final gameplay folder contains forest/dark-forest/swamp `capture.png` files; the
earlier gameplay folder contains ravine, snowy mouth, dry interior and high-camera
climate captures. Maps and cutaways are in
`build/v12-verified-audit/audit`; final packaged audit output is recorded by
`build/release2112-test-root.txt`. Each timing run records its executable SHA256.

Hardware: Intel i7-14700F (28 logical processors), AMD Radeon RX 9060 XT, 32 GB RAM,
Windows, MSVC Release. Low quality constrains workers on this machine; it is not a physical
dual-core or integrated-GPU benchmark. Driver scheduling and single-operation stalls remain
outside the soft budgets. No claim of universal hardware performance is made.
