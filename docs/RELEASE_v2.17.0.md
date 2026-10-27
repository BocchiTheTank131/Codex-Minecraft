# Voxel Frontier v2.17.0 — The Logic & Automation Update

**Stable release — includes the validated prerelease stabilization fixes.**

**Known limitation:** large-coordinate movement and targeting lose precision near
±8,388,608 blocks and beyond. The coordinated precision migration is deferred.

- Build circuits with directional power wires, levers, timed buttons, pressure
  plates, repeaters and AND/OR/NOT gates.
- Power lamps, doors, trapdoors and fence gates; inspect signal state in F3.
- Move real inventory stacks through directional, power-lockable hoppers, including
  furnace input/fuel/output routing. Existing metadata, sorting and container UI remain.
- Push up to twelve eligible blocks with pistons; sticky pistons pull one back.
  Obstructions and excluded block entities cancel movement safely.
- Use daylight, observer and crop sensors with automatic harvesters and hopper
  storage. Replanting consumes real seeds/roots and unloaded farms do no offline work.
- Fourteen new craftable items have original pixel textures, native 64px previews,
  contextual hints, placement ghosts and Recipe Book integration.

All generators V1–V22, historical IDs, terrain textures, four billboard mobs,
checkpoint protection and v2.16.2 quality-of-life systems are preserved.
Windows full-game installer and stateless standalone demo are included with SHA256SUMS.txt.
See [validation](LOGIC_AUTOMATION_VALIDATION.md) for rules, controls, measurements,
compatibility checks, screenshots and limitations.


Validation: 689 full-game automation assertions, 660 demo assertions, 20 packaged
regression suites, and 1,326 unchanged historical generator hashes. Desktop
benchmarks cover RD8/16/32 and a brief RD64 stress run; physical low-end hardware
was not available.

Known limits: no comparator, loose-item suction, piston animation, or block-entity
movement. Very large networks can respond more slowly under the bounded work budget.
Hoppers reuse 27-slot storage. Full rules and measurements are in the validation report.

## Prerelease stabilization fixes — 27 October 2026

- Protect item ownership when live drops or checkpointed recovery storage are full.
- Reserve complete multi-block placements before editing terrain or consuming items.
- Harvest sugar cane from the top; retain unsupported crop outputs safely.
- Correct powered door edge tracking, legacy paired doors and suspended device delays.
- Reject dying breeding partners and preserve fast Recipe Book text input.
- Bound registry reconciliation, signal-wave preparation and stale write disposal;
  report total automation tick costs in F3.

Large-coordinate precision remains **deferred and reproduced**. The release includes
375 additional stabilization checks, the original 689 automation assertions, 20 full/demo
Release suites and 1,326 unchanged historical generator hashes.
See [stabilization validation](V2170_PRERELEASE_FIX_VALIDATION.md) for evidence,
package checksums, before/after reproductions and remaining limitations.
