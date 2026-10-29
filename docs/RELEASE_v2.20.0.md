# Voxel Frontier v2.20.0 — Villages & Living World

Windows release. Publication approved by the owner. No installation or real-save modification was performed.

- Four persistent villager professions with five barter offers each. Four offers
  start available; the community stall unlocks the fifth. Reputation modestly
  changes eligible prices. Offers show required quantities and remaining stock.
- Existing generated villages gain stable identities without terrain generation
  changes. Existing and summoned villagers retain their original entity type;
  independent travelers can trade without belonging to a settlement.
- Work, evening gatherings, night rest, weather shelter and threat reactions use
  existing time, collision and animation systems. Local pathfinding has bounded
  searches and retry backoff; villagers never teleport to their destinations.
- Three persisted village requests drawn from delivery, seed supplies, public
  works and assistance. Accept, abandon or claim them through a villager. Rewards
  are granted once. Journal Challenges shows completed village requests/projects.
- Fund and explicitly confirm two public improvements: four paved road cells
  and a small fenced market stall with an empty barrel. Construction validates
  loaded, clear cells and occupants before atomic edits. Unspent funding can be
  refunded; completed projects never automatically regenerate.
- Village-specific Untrusted, Neutral, Friendly, Trusted and Honored reputation.
  Ordinary trade reputation has a lifetime cap; attacks and damage to designated
  completed public works have restrained, attributable penalties.
- VLG1 village state and VIN1 NPC associations join the existing immutable
  checkpoint. Finite stock, requests, reputation, funding, world edits and player
  inventory commit together. Older saves initialize missing village data safely.

## Controls

Right-click a living villager to open the dialog; Sneak bypasses conversation for
ordinary placement/use. Escape closes. Left/Right or Tab changes category;
Up/Down selects a row; Enter activates it. Mouse clicks select a row, then clicking
the selected row activates it. Click category labels directly.

Requests: Enter accepts or claims, Backspace abandons, R posts the next set only
when every current request is finished/abandoned and 600 active seconds elapsed.
Projects: Enter contributes available needed materials, configured Sneak + Enter
confirms construction, Backspace refunds unfinished contributions if they all fit.
The project status gives the proposed/completed coordinates or an obstruction
message. Construction never proceeds merely because funding is complete.

Trades and contributions use plain compatible resources from unlocked player
slots. Equipment, offhand, crafting grids, cursor stacks, item identity and tool
durability are retained. Output must fit in unlocked slots before inputs move.

## Compatibility and scope

Generators V1–V24, existing village blocks/chests/loot, all 365 item previews,
original hostile billboard definitions/art and prior gameplay remain compatible.
No new generator, item IDs, texture assignments, independently written village
sidecar or changes to transactional save coordination. The standalone demo keeps
village progress only for the current session.

Farmer work and requests use seed provisioning; villagers do not harvest player
crops. No paid tool repair, NPC building animation, new character models or
autonomous town rebuilding is added. Resting uses the existing idle model rather
than a bed animation. Very obstructed routes can fail gracefully. Completed
development unlocks remain recorded even if the player later demolishes the
improvement. See the validation report for exact evidence and untested areas.

Local files: `VoxelFrontier-v2.20.0-Windows-Setup.exe` (full persistent game),
`VoxelFrontier-v2.20.0-Windows-Standalone.exe` (stateless demo), and `SHA256SUMS.txt`.
Do not execute the installer as part of validation.
