# v2.17.0 prerelease stabilization validation

Local stabilization of the **published v2.17.0 candidate**, commit
`396edb629a947097ed372d6b49e0d6ef28fa8a22`, on 27 October 2026.
The version remains 2.17.0. On 27 October 2026 the user approved publishing these
validated changes as the official v2.17.0 release, replacing the earlier prerelease.
The evidence records the pre-publication validation snapshot. No installation or
real-world save modification was performed. All native fixtures create separate
temporary profiles.

## Findings

| Finding | Status | Root cause, change, and evidence |
|---|---|---|
| 1. Item ownership at output limits | **Fixed for audited production paths** | Gameplay/GUI drops extracted stacks before checking `spawnDrop`; boats, shearing and resource interactions consumed their source regardless of admission. `Game.cpp`, `UIManager.cpp`, `Transportation.cpp`, `Farming.cpp`, and `Survival.cpp` now commit the original owner only after acceptance. Destructive mining, explosion and support-cleanup paths reserve complete output batches in the existing world-checkpoint recovery queue. `World.cpp` also protects populated block-entity replacement directly. Queue capacity and stack validation match the loader; failed batches leave the queue unchanged. Tests cover both limits, metadata, deferred delivery and checkpoint ownership. |
| 2. Atomic multi-block placement | **Fixed** | Individual capacity checks admitted each half against the same pre-placement count. `Automation::canRegisterPlan` checks the net registrations for the entire unique-position batch. `World::automationBatch` validates all expected states, loaded positions, valid states and registry capacity before mutation; populated block entities cannot be moved/replaced through this mechanism. Removals release registrations before additions. Building placement and legacy door input use this batch; consumption follows success. Actual RMB tests cover both door families at 99,998 / 99,999 / 100,000. Existing bed and painting building regressions remain in place. |
| 3. Sugar cane | **Fixed** | Harvesting the middle detached the top; later farming cleanup erased it. The harvester selects the uppermost segment before validating copied output slots and applying the edit. The base stays rooted. Unsupported crop cleanup now retains its own outputs before deleting a plant. Tests cover two/three segments, different harvester heights, competing harvesters, full outputs, rooted bases and saved container totals. |
| 4. Remembered door input | **Fixed** | Input was remembered only after a visible state write, using the physical open bit. A manual close followed by power-off had no write and failed to acknowledge the edge. Electrical input is now committed explicitly even when the desired physical state already matches. Paired state and remembered input share one main-thread operation. Held power still permits manual override; the next electrical edge re-arms the mechanism. |
| 5. Legacy wooden-door synchronization | **Fixed** | Legacy halves evaluated independent adjacent power. The lower half now owns the mechanism, aggregates both halves' inputs and updates both halves atomically. Original facing, hinge and legacy IDs are retained. Incomplete or mismatched pairs are left untouched. Upper/lower power and repeated manual-override cases pass. |
| 6. Suspension and timing | **Fixed for tested range/port transitions** | Reconciliation erased active membership without freezing deadlines; expired inactive timers restarted at one tick, and chunk activation could resume incomplete boundary nodes. Timers now count eligible 10 Hz ticks using a bounded range-history ring, while unloaded-port notifications preserve remaining ticks. Activation/reconciliation use the same readiness checks, including sensor fields and machinery ports. Remaining delays persist through AUT1 without changing its encoding. A follow-up reproduction also exposed distance-setting invalidation of the initialized center: `World.cpp` now retains the center while `WorldStreaming.cpp` separately refreshes invalidated request radii. |
| 7. Large-coordinate precision | **Deferred — reproduced, still present** | Movement stops at float-scale coordinates around ±8,388,608 and ±30,000,000. The requested ±29,999,999 inputs themselves round in the current float API. `Player` positions, collision/targeting APIs, world position save fields, boat/navigation positions and absolute float camera matrices share this representation. Converting only player storage would immediately quantize movement again at these boundaries. A safe double-coordinate / relative-rendering migration needs coordinated API and codec work plus visual validation, beyond a small isolated stabilization change. No partial precision conversion was shipped. |
| 8. Breeding with dying animals | **Fixed** | The selected animal lacked a positive-health check, and the partner search omitted health/death-state checks. Both partners must now be living adults with valid cooldown/love conditions. A dying partner with an active love timer cannot produce offspring. No hostile mob definitions or behavior were altered. |
| 9. Recipe Book typing | **Fixed** | `firstPressedKey()` selected one key rather than the queued text events. Recipe search now consumes the ordered character queue once, validates the existing letters/spaces and 18-character limit, and handles focus, Enter, repeatable Backspace/Delete and sanitized paste separately. Creative/sign consumers remain separate. Tests exercise multiple characters in one frame, invalid characters, repeat deletion, clipboard text and focus switching. |
| 10. Automation preparation budget | **Fixed for identified passes** | Full registry reconciliation and dirty-set copying occurred outside the evaluation limit. Reconciliation now visits at most 512 registrations per tick, with a fair cursor; a signal wave starts with a constant-time set swap. Existing limits of 2,048 due entries, 2,048 weighted evaluations and 512 state writes remain. Rejected/stale write attempts also consume a 512-attempt budget. Total timing includes preparation, evaluation and commits, exposed in F3 alongside reconciliation/due counts. Sparse 100,000-registration and 10,000-stale-write tests verify bounded progress. |

These statuses describe the audited paths and recorded tests, not a claim that
all possible defects are eliminated.

## Ownership and compatibility rules

- Q/Ctrl+Q, actual inventory slots, equipment/cursor ownership and outside-GUI
  drops use a candidate inventory. Admission failure leaves the original intact.
  Creative catalog availability is still unlimited.
- Boat recovery refuses a break when its item cannot be spawned. Sheep retain
  wool and tool durability on refusal. Composter/berry state and chicken egg
  cooldown advance only after output admission.
- Mining and explosion batches retain exact item/count/durability/map identities
  before clearing storage. Natural animal death loot is admitted as a complete
  batch or retained by the world; at both limits a loot-bearing death is delayed
  instead of discarding its resources. Billboard mobs have no new loot behavior.
- Recovery retains the existing **100,000-stack** bound. No silent overflow,
  unchecked append, or unbounded emergency sidecar was added. At complete
  saturation, an unsupported decoration may remain placed until it can be
  recovered or edited again; keeping its owned contents takes priority over
  deleting it. Resource production applies backpressure rather than refunds.
- Recovery remains in the existing BDP1 world component, automation uses the
  existing AUT1 records, and containers use existing inventory records. No new
  IDs, terrain generator, save format, checkpoint bypass or asynchronous-save
  redesign was introduced.
- World mutations and ownership changes run on the main thread. Save capture
  cannot interleave halfway through a batch. Immutable background checkpoint
  commits, verification, journaling, session reservations and explicit recovery
  remain unchanged.
- Normal wires still attenuate; devices retain their existing recipes, textures,
  orientations, mechanics and simulation rate. Manual interaction and power
  transitions use the prerelease's intended edge-based override behavior.

## Reproductions and regression procedure

`tests/stability/automation.cpp` adds `--automation-stabilization` and
`--automation-sparse`; `tools/run_automation_validation.ps1` now includes the
stabilization suite. The original candidate was archived separately and rebuilt
with test-access friendship only for corrected baseline fixtures. It reproduced
**15 independent failing checks**. The initial exploratory run had 16 failures;
one cane fixture lacked valid rooted support and the range fixture initially
invalidated its own center through the settings setter. Those fixtures were
corrected and the original candidate was rerun; use `before-corrected.log`, not
the exploratory count, as the baseline.

Additional focused runs reproduced the distance-setting bypass and unbounded
stale-write discard before their fixes. The extended native fixture itself grew
too large for comfortable stack use: 16 large World fixtures were moved to heap
allocation, matching production ownership. This corrected a test-process crash;
no renderer workaround was applied.

The final stabilization tests include:

- Actual gameplay Q, GUI Q/Ctrl+Q in Survival/Creative, outside left/right click
  and drag release, successful single acceptance, tool durability and map identity.
- Full live drops; full deferred queue; partial/batched refusal; storage removal;
  barrel/hopper/frame explosions; sheep, berries, compost and boat recovery.
- Complete and coalesced background checkpoints while output ownership changes;
  exact-once delivery; a verified previous-checkpoint recovery into a separate
  copy; aborted staged metadata preserving the verified prior generation.
- Both door types through actual RMB at the three capacity boundaries; existing
  multi-block building checks; signal/manual override re-arming and legacy halves.
- Cane container capacity, two harvesters at different heights, rooted bases and
  saved output totals.
- Repeaters, buttons, observers and daylight sensors through three neighboring
  unload/reload cycles, incomplete boundary activation and timer serialization;
  simulation-distance suspension and the settings initialization regression.
- Dying breeding partners, ordered Recipe Book text, paste validation, focus,
  repeat deletion, 100,000 sparse registrations and stale-write budgets.

Large-coordinate sampling is a **failure record**, not a passing movement test.
The `precision.csv` samples 120 Spectator integration steps at both signs of
1,000,000, 8,388,608 and the requested 29,999,999. Ground collision/targeting and
large-coordinate save precision remain unverified because the migration is
explicitly deferred.

## Final measurements and artifacts

Final test counts, historical hashes, package verification, checksums and matched
CPU measurements are recorded in the evidence section below. Benchmarks compare
against v2.17.0, not v2.16.2. No main-thread autosave optimization, v2.17.1 work,
GPU speedup claim or lower render-distance setting is included.

### Remaining limits

- Large-coordinate movement/targeting is still unsafe at float precision limits.
- Very large networks can incur extra response latency while bounded discovery,
  evaluation and commit queues converge. The range-history ring conservatively
  freezes a timer if its expected bounded processing history is ever exceeded.
- Chunk activation/unloading still performs chunk-local registration/suspension
  work. This pass bounds the identified tick-wide preparation passes; it does not
  claim all possible streaming callbacks have a constant wall-clock duration.
- Full queue backpressure can temporarily preserve unsupported decorations and
  delay loot-bearing animal death. There is no infinite overflow storage.
- CPU microbenchmarks on the available desktop are not low-end hardware or
  RD8/16/32 gameplay FPS benchmarks. No new visual or world-generation work was
  needed. The installer is built locally; it is not run against the real install.

## Final evidence

The final focused run passed **689 existing automation assertions**, **375 new
stabilization checks**, **660 demo automation assertions**, **4,774 building**,
**3,474 actual Creative UI**, **1,389 QOL**, and **261 farming** assertions.
The original 260 farming checks are retained; the additional check verifies that
the fixture actually moved its streaming center to the farm after teleporting.
The old fixture had relied on changing distance to disable range eligibility.
That dependency was corrected using the production streaming update, without
weakening crop assertions. The historical farming benchmark fixture was aligned
with its intended test center for the same reason.

All **20 Windows Release suites** passed (11 full / 9 demo), including transport,
commands/permissions, Survival, weather, fluids, reset, Spectator, menus,
optimization, legacy regressions and UI as applicable. Native subsystem, Game,
checkpoint corruption, background commit and interrupted-save recovery tests also
passed. Both variants match **663 historical hash fixtures each: 1,326 total**,
covering all generators V1–V22. The demo wrote no persistent world components.

The cached 361-item atlas, world textures, item/block ID mappings and four
billboard sprite files are unchanged. Both binaries embed the exact atlas hash
`4b910fecaa9f4f6f5f99336da40d7be82bc5bbd231cd3dc0264efd50ad6ac03d`.
The required source manifest was regenerated; missing/stale manifest rejection
checks still pass. No inventory art was altered to obtain a passing build.

### Matched CPU comparison

Intel Core i7-14700F, 28 logical processors; three alternating original/current
runs. The CPU fixture uses 400 warm-up ticks and 300 measured updates for each
100 / 1,000 / 10,000 component workload; the sparse 100,000-registration fixture
measures all 220 updates including initial reconciliation. Timings cover complete
`Automation::update` calls. Raw per-phase timing is also retained.

| Workload | Original mean ms | Stabilized mean ms | Original p99 ms | Stabilized p99 ms |
|---|---:|---:|---:|---:|
| 100 toggling | 0.0407 | 0.0410 | 0.1637 | 0.1302 |
| 1,000 toggling | 0.1843 | 0.1711 | 0.6922 | 0.6129 |
| 10,000 toggling | 1.1177 | 1.0645 | 2.5278 | 2.2130 |

Values are medians of the three run means/percentiles, not pooled percentiles.
The active-circuit differences are modest and do not establish an FPS speedup.
In the sparse-registration stress case, median p99 fell from **0.8464 ms to
0.0844 ms** and median worst tick from **1.5321 ms to 0.0919 ms**. This validates
removing the periodic full preparation pass on this machine. Settled idle values
are close to timing resolution; no huge idle-speedup ratio is claimed.

The final budgets are 512 eligibility checks, 2,048 due entries, 2,048 weighted
evaluation units, and at most 512 commit attempts / state writes per tick. These
are separate bounds, not a claim that all work fits into 2,048 operations. Signal
wave preparation swaps ordered sets rather than copying the dirty population.
F3 exposes total tick and preparation/evaluation/commit times; total update timing
also includes maintenance. Timers add transient bookkeeping only; saved component
sizes remain unchanged for identical registered states. No GPU, frame pacing,
low-end hardware, or RD8/16/32 FPS improvement is claimed from these microbenchmarks.

Re-run focused tests with:

```powershell
./tools/run_automation_validation.ps1 -SkipBuild -Output build/v2170-stabilization/recheck
./tools/run_stability_regressions.ps1 -SkipBuild -IncludeDemo -Output build/v2170-stabilization/release-recheck
```

`tools/run_prerelease_automation_benchmarks.ps1` accepts an explicitly archived
original-v2.17.0 native driver and alternates three matched runs. Its method file
records both driver fingerprints. `tools/summarize_prerelease_validation.py`
verifies hashes, assertions and package checksums before retaining compact evidence.

### Local packages

Both Windows Release packages were rebuilt after the final production changes.
The standalone package and full-game executable passed the suites above. Inno
Setup successfully compiled the installer using the full-game binary. The real
installer was **not executed** and no real install or saved world was upgraded.

- `VoxelFrontier-v2.17.0-Windows-Standalone.exe` — 76,785,152 bytes; SHA-256 `683fea61db9e9f610964eb814bc057636b1ff2ee22ee3513a1c07fec178eb877`.
- `VoxelFrontier-v2.17.0-Windows-Setup.exe` — 71,548,671 bytes; SHA-256 `cb45cfc40cd2dc4f7d84a42aa0fc1df16668b93ef74f25bbd24716d255c62355`.

Evidence: [summary](prerelease-v2170/summary.json),
[corrected original reproductions](prerelease-v2170/before-corrected.log),
[stabilization checks](prerelease-v2170/stabilization.log),
[original automation regressions](prerelease-v2170/automation.log),
[demo automation](prerelease-v2170/demo-core.log),
[Release suites](prerelease-v2170/release-suites.csv),
[checkpoint tests](prerelease-v2170/checkpoint.log),
[subsystem stress](prerelease-v2170/subsystems.log),
[benchmark summary](prerelease-v2170/benchmark-summary.json),
[benchmark method](prerelease-v2170/benchmark-method.json),
[precision failure record](prerelease-v2170/precision.csv),
[embedded assets](prerelease-v2170/embedded-previews.json), and
[checksums](prerelease-v2170/SHA256SUMS.txt).
