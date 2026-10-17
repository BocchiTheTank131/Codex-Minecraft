# v2.14.3 checkpoint regression evidence

See [the report and recovery instructions](../CHECKPOINT_v2.14.3_VALIDATION.md).
All profiles were isolated. No user's real save was repaired or overwritten.

- `repro-before.log`: actual v2.14.2 Game save/mode-change reproduces self-session
  conflict and `world_seed.txt` integrity failure. The assertion fails before fixes.
- `checkpoint.log`: final focused driver, 80 assertions. Mode/time/weather changes,
  background capture/coalescing, cheats permissions, shutdown/reload, write guards,
  permanent-error suppression, separate verified recovery, missing/invalid backup
  refusal, pre-manifest compatibility, root legacy import and source preservation.
- `subsystems.log`, `game.log`: prior v2.14.2 stability regression paths still pass.
- `regressions.csv`: 11 full plus 9 demo suites, all exit 0.
- `full-*.log`, `demo-*.log`: per-suite production logs. Deliberate file locks,
  mid-commit termination and corrupt fixtures produce expected error messages;
  inspect the final suite result rather than treating those as unexpected failures.
- `full-transport.log`: 400 concurrent writes, 200 paired checkpoint commits,
  data/metadata interruption before manifest replacement, journal rollback with
  retained verified backups, transient locks and actual Game save/retry paths.
- `full-chunk-hashes.csv`, `demo-chunk-hashes.csv`: 663 records each match the
  existing `docs/transportation-polish/chunk-hashes.csv` baseline exactly.
- `music-tracks.csv`: exact source filenames and SHA-256s; every file occurs in
  full/demo actual playback logs. Both report 24 unique tracks through automatic
  transitions and the shuffle boundary, without category-gain failures. This test
  seeks near track endings; it is not full-duration listening evidence.
- `package.log`: final full/demo Release and local installer packaging output.
- `SHA256SUMS.txt`, `validation.json`: exact final distribution hashes, sizes,
  version, reproduction status, profile checks and limitations.

The initial matrix was repeated after the final diagnostic/interruption changes.
`checkpoint.log` was rerun after adding the root-import fixture, using the same
final production objects. No implementation changed after the final package build.
The source driver is tracked under `tests/stability/`; the runner derives v2.14.3
paths from CMake. Builds and larger fixtures remain under ignored `build/`.

Publication was subsequently approved for v2.14.3. The existing song display/audio
changes from the prior request are preserved; this correction adds no renderer or
world-generation changes.
