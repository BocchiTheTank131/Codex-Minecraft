# v2.14.2 retained evidence

See [the validation report](../STABILITY_VALIDATION.md) for interpretation and limits.
This directory is a small reviewable subset of the isolated `build/v2142-validation/`
evidence. No real user profile is included. Build and test code are tracked in
`tests/stability/`, `tools/run_stability_regressions.ps1`,
`tools/run_stability_benchmarks.ps1` and `tools/test_stability_installer.ps1`.

## Correctness

- `baseline-subsystems.log`, `baseline-game.log`: reproduced original v2.14.1
  defects using production objects. `before_mode health=13 hunger=10` followed by
  the original mode setter demonstrates the heal/reset defect.
- `subsystems.log`, `game.log`: final isolated production-path negative tests,
  high-altitude/falling-drop roundtrips and complete 20-minute clock arithmetic.
- `keyboard-before.log`: newly exposed R recovery control was initially below a
  mouse-only return and failed. `game.log` records the corrected keyboard path.
- `high-altitude-before.log`: stricter position validation initially rejected a
  legitimate high-altitude save; corrected before final packaging.
- `regressions.csv`: 11 full and 9 demo Release suites, all exit 0.
- `full-*.log`, `demo-*.log`: per-suite logs, including deliberate lock failures.
  Those failures are expected negative cases; the final suite outcome must pass.
- `full-chunk-hashes.csv`, `demo-chunk-hashes.csv`: 663 exact matches each against
  `docs/transportation-polish/chunk-hashes.csv`, generators 1-22.

## Measurements

- `performance.csv`: 72 per-phase rows, two alternating-order repeats per version,
  RD8/16/32 and gameplay/ocean scenes. Collected before final clock/menu/validation
  refinements; no general speedup established. `performance-summary.json` averages
  these per-phase/run metrics, rather than pooling frame percentiles.
- `streaming-rd32.csv`, `retirement.csv`: focused flight/teleport profiling; large
  retirement sample localized to page trim/destruction, still unresolved.
- `survival-rd*.csv`, `timing-rd*.csv`, `traversal-rd*.log`: final foreground
  production Survival walking/boating, imposed frame rates, 500 ms spike and reload.
  Timing accounting includes carried debt. These are not uncapped FPS comparisons.
- `rd64-smoke.csv`: brief unpaired high-distance stress check.
- `final-rd16.csv`: final unpaired payload rendering smoke.
- `survival-taiga.png`, `survival-boat.png`: inspected actual in-engine screenshots.

## Packaging and safety

- `package.log`: both Release variants and local installer packaging completed.
- `SHA256SUMS.txt`: installer/demo distribution hashes.
- `validation.json`: exact final full/demo/setup hashes and sizes, results, status
  for every audit finding, timing error, demo checks and explicit unverified limits.
- `installer.json`: unique-AppId isolated baseline-to-final upgrade/uninstall
  check. Save hashes survive installer actions, old worlds load, final payload
  matches, production installation registration remains unchanged.

The standalone demo rejected `--save-interruption-fixture=...` with exit 1 and
created no fixture directory. All nine demo suite profiles were scanned for world,
inventory, player, mob, navigation, checkpoint and world-session files; none existed.
The installer used a synthetic AppId so the user's installed release was untouched.
Publication of v2.14.2 was subsequently approved by the user; validation preceded release.
