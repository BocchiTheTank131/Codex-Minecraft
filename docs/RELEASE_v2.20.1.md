# v2.20.1 — Village Polish & Living World Improvements

Windows release; publication approved by the owner. No installation or real-save modification was performed.

Pigs and cows also receive distinct voxel models, movement-driven diagonal
strides, smooth turning, young-animal proportions and body collision/step jumps.
See [livestock validation](LIVESTOCK_VISUALS_v2.20.1.md) for captures and tests.


- Original upright villagers with faces, paired limbs and profession clothing:
  straw hat, smith apron, librarian book and merchant pouch.
- Movement-driven footsteps and limb motion, smooth turning, conversational
  facing, breathing and lightweight trading/work/social gestures. Shared OpenGL
  3.3 instancing batches the new model parts without per-NPC graphics resources.
- Upright targeting and outlines match the villager collision body. Sheep, chickens,
  pillagers and the four original billboard enemies retain their own models.
- Larger village dialog and trade names, current usable item counts, stock,
  discount and transaction-backed availability. Explicit mouse action buttons
  select separately from purchase; keyboard controls remain supported.
- Better merchant exchanges: 24 cobblestone for 8 logs, 12 logs for 4 cooked beef,
  1 iron for 6 coal, and 2 iron for 24 torches. Prices retain existing reputation
  rules and finite stock; the unhelpful boat exchange is replaced.
- Restock eligibility/supply requirements, request completion and reputation
  milestones are clearer. Existing stock deadlines and request objectives persist.
- Newly funded Community Stalls have a slab canopy, counter and lamp. Merchants
  use their approach during work hours. Existing completed stalls are preserved.
- Pending project geometry is previewed through the existing placement renderer.
  Press W in the village dialog to track the village turn-in or selected project.

Project funding, costs and identities remain unchanged. Containers are never
refilled by the stall. No generated terrain, save schema, item IDs or preview art
is intentionally changed. See the validation report for actual verification,
performance measurements and remaining limitations.

Controls: RMB on villager to open; Sneak preserves placement. Left/Right/Tab
changes tab; Up/Down selects; Enter trades/accepts/claims/contributes;
Sneak+Enter builds; Backspace abandons/refunds; R rotates eligible request sets;
W adds/selects a waypoint; Escape closes. Mouse rows select; labeled buttons act.

Downloads include the full persistent-game installer, stateless standalone demo
and SHA-256 checksums. Source code is available from the release tag.

Passive wildlife now spawns twice as frequently (10-second cooldown instead of
20 seconds), with the passive population cap increased from 26 to 52. Existing
group size, habitat rules and previously populated-chunk tracking remain.
Hostile spawn chance, cap and nighttime bonus are unchanged. Actual populations
still depend on suitable loaded terrain.

Validation: 15 functional groups and 20 full/demo Release suites passed in the
village/livestock integration. Historical hashes and protected previews matched.
The final passive-spawn balance change additionally passed farming and full/demo
Survival checks. See [village validation](VILLAGE_POLISH_VALIDATION_v2.20.1.md),
[livestock validation](LIVESTOCK_VISUALS_v2.20.1.md) and
[spawn balance validation](PASSIVE_SPAWNING_v2.20.1.md) for exact test stages,
benchmarks, screenshots and package fingerprints.

Remaining scope: paid equipment repair, new request variants, peaceful assistance
alternatives, lying-down sleep poses and automatic upgrades to existing stalls
are not included. Existing progress and completed construction are preserved.
