# v2.15.1 — Survival statistics and wheat XP

## Reproductions

Both failures reproduced against v2.15.0 production objects using the tracked
isolated farming driver before production fixes:

- Survival values 9 health / 12 hunger / 3 saturation, followed by Creative
  simulation, failed the preservation assertion. The per-frame movement branch
  assigned health and hunger to 20 even though mode setters preserved them.
- Production `Game::updateMining` on ripe wheat failed the 2-XP assertion. The
  v2.15.0 shared item-harvest path omitted the former wheat XP branch.

## Changes

- Removed the Creative movement health/hunger assignments. Creative damage
  immunity and Survival-only exhaustion/regeneration remain in their existing
  paths; no new per-frame work is added.
- Restored a 2-XP orb reward specifically for `Crop3` in the Survival mining path.
  Existing crop items and ore XP are unchanged.
- Version set to 2.15.1. No generator, ID, save format or mob changes.

## Regression coverage

- Actual production mode setters and player simulation: five seconds in Creative,
  a second of flight, five seconds in Spectator, another second in Creative, then
  return to Survival. Health, hunger and saturation must stay 9 / 12 / 3.
- Production mining: ripe wheat awards exactly 2 XP; a repeated mining call does
  not award it twice; three immature wheat stages and ripe carrots award none;
  Creative ripe-wheat removal awards none.
- Existing nutrition persistence and farming/checkpoint tests remain included.

## Validation results

- Both original defects failed reproducibly before the fixes and passed afterward.
- Final Release-linked farming driver: **260 assertions passed**, including
  production mode-switch simulation, flight, wheat mining and nutrition reload.
- Production subsystem, Game and checkpoint drivers passed.
- Full-game and standalone-demo Release builds completed with product version
  **2.15.1**. Local installer and standalone packages were generated.
- **All 20 Release suites passed:** 11 full-game and 9 stateless-demo suites.
  These include Survival, fluids, commands, transport, weather, UI and optimization
  smoke coverage; demo persistence checks also passed.

Evidence: `build/v2151-validation/` (before-health.log, before-wheat.log,
functional.log, subsystems.log, game.log, checkpoint.log and regressions/results.csv).
Tests use isolated profiles, never real player saves. Publication was approved by
the user. No new performance claims or historical chunk-hash claims are made
for this small gameplay patch.
