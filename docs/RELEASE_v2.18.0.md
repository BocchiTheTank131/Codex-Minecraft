# Voxel Frontier v2.18.0 — The World Generation & Biome Expansion Update

Generator V23 creates connected regional ridges, rocky escarpments, mountain passes
and eroded uplands. Compatible habitats vary within coherent climates, with mixed
woodland, clearings, taller occasional trees, and clustered ground vegetation.

Underground, wider connected cave fields combine chambers, tunnels, vertical veins
and supporting columns. Rare aquifers form underground lakes. Surface approaches
must reach native cavities and pass dense walking-floor checks before acceptance.

Five additional landmark designs reuse existing materials and persistent loot:
wilderness camps, lookout towers, standing stones, mountain shelters, and forest
sanctums. Their regional planner checks slopes, support, reservations, and the
natural height ceiling, including pieces crossing chunk borders.

New worlds default to V23. Existing worlds keep their saved generator V1–V22 and
their player edits. No item/block IDs, save layout, automation devices, mob models,
or world-coordinate representation have changed. The standalone demo remains
stateless. Existing 361-item previews and original textures are retained.

Validation passed 20 full/demo Release suites, all 1,326 historical generator hash
records, 24,187 full-game generation checks, 24,095 demo generation checks, and 50
paired Survival spawn seeds. Native screenshots and V22/V23 maps cover three
seeds at 4,096, 15,360 and 46,080-block scales.

See [the validation report](GENERATOR_V23_VALIDATION.md) for the exact test matrix,
native landscape captures, historical hashes, measured costs, and limitations.

Known limits: V23 costs more to generate than V22, especially on cold regional
requests. Matched RD32 streaming runs recorded about 9% higher mean frame times;
occasional long frames and RD64 streaming/retirement limits remain. Measurements
come from one development PC, without physical low-end hardware certification.
Its drainage follows the existing geographical graph rather than a
physical erosion simulation. Cave connectivity is tested in bounded volumes and
walking routes; an arbitrary seed is not guaranteed to have one global cave
network. The pre-existing large-coordinate movement precision limitation remains.
Expanded exposed overhangs have not been visually verified and are not claimed
as a completed feature of this version. Fallen logs remain deferred; supported
stumps are included.

Downloads: `VoxelFrontier-v2.18.0-Windows-Setup.exe` (persistent full game),
`VoxelFrontier-v2.18.0-Windows-Standalone.exe` (stateless demo), and `SHA256SUMS.txt`.
