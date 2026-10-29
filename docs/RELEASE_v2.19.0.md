# Voxel Frontier v2.19.0 — Exploration & Relics

Windows release. Publication approved by the owner; no installation or real-save modification was performed.

- Craft a Surveyor Compass from a Surveyor Token; find eligible nearby planned
  sites without generating distant terrain, and use the existing waypoint HUD.
- Turn a Cavern Shard into an Echo Lantern: 64 supported, persistent cave lights,
  a two-second use cooldown, a loaded-only Sneak+RMB cave echo, and ordinary
  block lighting rather than a moving light.
- Use an Ancient Seal as a reusable vault key. Combine a Seal, Resonant Core,
  diamonds and an iron pickaxe for the 1,024-durability Ancient Explorer Tool.
- Open the configurable Adventure Journal (default J) for discoveries, challenges,
  relic progress and completed achievements. Left/right changes category;
  up/down changes page. Exploration continues while the journal is open.
- New V24 worlds contain Three Bells sequence puzzles, Divided Current AND-gate
  circuits, and Pulse Hall pressure-plate trials. Follow the numbered yellow
  inlays; avoid the red floor while the warning lamp charges.
- Defeat a finite two-guard ambush or survive a ten-second telegraphed pulse trial.
  Vault ownership, challenge progress and one-time XP survive checkpoint reloads.
- Four appended item IDs/previews; all 361 historical previews and original world
  atlas pixels are unchanged. No new mob, renderer pass or world-height change.

Paid recipes for all three original relics also support pre-structure worlds
without rerolling any chest or creating a free crafting loop.

Older worlds keep their original generator and exact generated loot. They gain
relic recipes and the journal, but are not retrofitted with V24 rooms. Missing
exploration save extensions initialize empty. New state is captured with the
existing complete checkpoint; no independently written journal sidecar is added.

Mining remains a sandbox escape route, including breaking vaults. Creative may
open vault containers without claiming Survival XP. Missing trial mechanisms
pause that challenge until restored. There is no Guardian, satchel, climbing
system, fourth puzzle archetype, ore detector or unlimited storage expansion.

Detailed native tests, package checks, performance evidence and limitations:
[Exploration & Relics validation](EXPLORATION_RELICS_VALIDATION_v2.19.0.md).

Review stabilization: bounded item/building lookups and registry-wide sorting
checks; hazard-aware bed exits and loaded-only bed hints; height-aware dungeon
plates/pulses; post-batch support cleanup with retained drops; actual piston
support dependencies; recipe-owned furnace progress, including immediate save
and hopper transfers; and Surveyor recharge preserving manual waypoint selection.
See [review evidence](V2190_REVIEW_FIXES.md). Coordinate precision remains deferred.
