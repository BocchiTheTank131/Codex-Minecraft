# Voxel Frontier v2.15.1 — Survival Statistics & Wheat XP

## Fixes

- Creative movement no longer resets Survival health and hunger to full. Health,
  hunger and saturation are retained through Creative/Spectator simulation and
  returning to Survival.
- Harvesting ripe wheat in Survival again awards **2 XP**. Immature wheat and
  Creative harvesting award none; repeated mining calls do not duplicate rewards.

## Compatibility and validation

No generator, block/item ID, save-format or mob changes.

Both defects were reproduced before the fixes. All **260 farming assertions**,
production gameplay/checkpoint tests and **20 Release regression suites** passed
(11 full-game, 9 demo), including mode transitions with simulation and flight.

## Downloads

- **Windows-Setup.exe**: installer for the full persistent game.
- **Windows-Standalone.exe**: stateless single-file demo.
- **SHA256SUMS.txt**: download checksums.

See [validation details](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.15.1/docs/VALIDATION_v2.15.1.md).
