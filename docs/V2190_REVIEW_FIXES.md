# v2.19.0 review corrections

Local uncommitted candidate. Real installations and saved worlds were not used.
The original exploration validation report describes the earlier candidate; this
report supersedes its package fingerprints and final test totals.

| Finding | Status | Change and evidence |
|---|---|---|
| 1. Relic definition lookup | Fixed | Bound automation items at AutoHarvester, excluding the internal piston head. Check all 365 IDs and sort inventories containing each registered item. The original probe confirmed invalid lookups, without deliberately dereferencing invalid memory. |
| 2. Cactus bed exits | Fixed | Reuse the safe-spawn floor predicate. A full solid cube and two completely clear body cells conservatively contain the centered player's entire contact area. All-cactus exits are rejected. |
| 3. Elevated plates/pulse | Fixed | Plate activation requires feet within its top activation slab; pulse damage requires floor-level feet as well as the marked horizontal cells. Elevated-player probes cover both. |
| 4. Gate support cleanup | Fixed | Extract existing ownership-safe support cleanup and invoke after all batch replacements. Carpet removal retains its item in checkpointed deferred drops. If recovery storage is full, the decoration retains ownership instead of being deleted; a subsequent support edit can retry cleanup. |
| 5. Piston neighbors | Fixed | Resolve the actual support coordinate for floor, ceiling and wall decorations. Unrelated flowers on separate dirt do not prevent movement; dependent decorations still prevent unsupported movement. |
| 6. Furnace recipe work | Fixed | Track the runtime input owning progress; reset on recipe changes. Save encoding writes zero stale progress even before the next simulation step; legacy decoding reconstructs ownership from saved input, without a new save format. |
| 7. Stale Surveyor selection | Fixed | Separate handled input from a successful new survey. Navigation changes only for a new successful result. Actual RMB regression preserves a manually selected waypoint during recharge. |
| 8. Bed hint generation | Improved | Both hints and bed interaction use loaded-only exit checks; only actual respawn permits missing-terrain sampling. Skip interaction calculation when hints and placement previews are both disabled. No prior stall was measured, so no frame-time improvement is claimed. |
| 9. Extreme coordinates | Deferred | Existing float precision limitation remains. No partial coordinate migration or claim of repaired movement. |

The review fixture initially reproduced definition, plate, gate-cleanup, furnace
and cactus failures. The initial piston probe also had an unrelated gate in its
push destination; that fixture obstruction was removed before validating the
independent-flower case. The user's separate piston reproduction remains the
before evidence; we do not count the obstructed fixture as proof of its root cause.

Code: Building.cpp/Building.h (bounded lookup and support coordinates),
Automation.cpp (post-batch cleanup and dependency checks), World.cpp/World.h
(spawn checks, support cleanup and furnace ownership), SaveSnapshot.cpp (safe
progress encoding), Adventure.cpp/Adventure.h and GameAdventure.cpp (contact and
survey results), GameBuilding.cpp/Game.cpp (loaded-only hints).

## Validation

<!-- REVIEW_RESULTS -->
Final verification: **380 full / 378 demo targeted review probes** passed, including all 365 item sorts. The harness additionally asserts the aggregate result. Existing exploration tests pass **215 full / 146 demo assertions**. The native Survival route passes **863 assertions**, including verified checkpoint reload.

**13 existing functional groups, 9 generation groups and 20 full/demo Release suites passed.** The new review suite ran separately in both variants and is now included automatically in future functional runs. All **1,326 historical hash records** and **495 exact V23 fixtures per variant** match. V24 deterministic/solvability and 50-seed spawn cohorts pass.

Embedded-resource checks pass for all 135 resources per variant. All 361 existing previews and 990 protected files remain exact; Automation.cpp is deliberately no longer classified as an unchanged source file because it contains the reviewed support fixes. Original billboard AI/art and historical terrain remain protected.

[Evidence index](exploration-v2190-review/summary.json), [review logs](exploration-v2190-review/full-review.log), [raw regression archive](exploration-v2190-review/regression-logs.zip), [package/source protection](exploration-v2190-review/package-assets.json), [checksums](exploration-v2190-review/SHA256SUMS.txt).

Rebuilt local packages (installer not executed):

- `VoxelFrontier-v2.19.0-Windows-Setup.exe`: 71,602,031 bytes; SHA256 `1380cdecaeca92860d02ddb9e0d617fa67e0e7aa6cadde86af527a3817adc898`.
- `VoxelFrontier-v2.19.0-Windows-Standalone.exe`: 77,358,592 bytes; SHA256 `8f547e149680c9ddcf4909b460a0ee2b7b6eb753460c852cf1aacb7ff9ba852b`.


The prior RD8/16/32 performance tables and screenshots remain evidence for the
previous candidate, not new measurements of these fixes. No new performance gain
is claimed. Save UI improvements, journal location selection/status improvements,
large-snapshot latency and RD64 driver work remain deferred rather than included.
No hostile billboard AI, art, terrain algorithm, item IDs or save headers changed.

Legacy furnace saves contain no original recipe identity beyond their current
input. Legitimate saved progress is retained; already inherited progress recorded
by an older executable cannot be distinguished retrospectively. New recipe
changes reset before cooking and before checkpoint encoding.

Reproduce the focused cases with:

```powershell
python tools/run_relic_fixture.py --mode review --output build/fresh-review
python tools/validate_relics_v2190.py --phase functional --output build/fresh-review-functional
```

The functional launcher now includes the review suite automatically. All fixtures
use fresh temporary working directories and marked isolated profiles.
