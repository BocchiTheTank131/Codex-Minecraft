# v2.14.3 checkpoint integrity correction

Validation, 2026-10-17. Publication subsequently approved by the user for v2.14.3.
An [additional settings/UI follow-up](V2143_SETTINGS_POLISH.md) was subsequently
built; the evidence and hashes below identify the preceding checkpoint validation.
This builds on the v2.14.2 stability changes and retains the previously requested
song-name display and updated audio. No terrain generator, billboard mob or renderer
change was made for this correction.

## Reproduction and cause

The new tracked `StabilityDriver --checkpoint` first ran against the actual v2.14.2
production objects in a fresh temporary profile. Saving a Survival world and then
switching to Creative reproduced both messages reported by the user:

1. `World session reservation failed ... .session.lock`.
2. `checkpoint-integrity ... world_seed.txt`.

`Game::setGameMode` called `saveWorldMetadata` directly. That function replaced
`world_seed.txt` without updating its checkpoint manifest, then called
`WorldLibrary::writeMetadata`. The latter attempted to reserve a world the current
Game already reserved. The second write failed while the first had invalidated the
manifest. The metadata serializer's result was ignored by the mode-change caller.
The [retained reproduction](checkpoint-integrity/repro-before.log) records exit 1
and `gamemode_preserves_manifest passed=0`. The reported message pattern was
reproduced through the actual Game save/mode-change path.

## Corrections

- Mode changes update live state only, matching existing time/weather command
  behavior. Full `saveAll` capture serializes metadata, player, inventory, terrain,
  containers, mobs, navigation and world information into one checkpoint.
- `saveWorldMetadata` refuses calls outside a checkpoint. The backend additionally
  refuses standalone writes to known managed filenames or any component listed in
  the current manifest. It rejects staging to another checkpoint directory and
  staging after failed checkpoint verification, before touching temporary files.
- One background commit remains allowed at a time. Overlapping autosave requests
  coalesce into one fresh capture after the pending commit. Manual saves, shutdown
  and world switching wait for pending work. Live changes never mutate staged bytes.
- World selection updates cheats through a lifetime reservation, fresh verified
  metadata read and coordinated metadata-only checkpoint. It changes the requested
  permission without replacing newer names, timestamps, mode/time or sidecars with
  stale menu data. Attempts against an open world remain refused.
- New-world metadata is created in one transaction. Legacy import synthesizes seed
  metadata only inside its private copy's transaction before publishing the folder.
- Verification identifies the mismatched component, manifest generation, expected
  and actual length/checksum. Background verification failures carry structured
  permanent status back to Game. Permanent failures disable automatic retries and
  further saves for that loaded session; manual reset cannot overwrite damaged data.
  A fresh verified load/recovery is required. Transient write failures retain bounded
  frame-deadline retries without production retry sleeps.
- Failed/interrupted rollback now retains the verified `.checkpoint-old` generation.
  Normal successful-checkpoint rotation still occurs only after current verification.
  No mismatched manifest is rewritten to legitimize unverified data.

The checksum format, journal, durable flush, atomic replacement, session reservation,
legacy codecs and world-generation formats remain in use. Checksums detect accidental
damage, not hostile tampering. A queued autosave is not a completed-save guarantee.

## Writer audit

| Path | Files/state | Coordination |
|---|---|---|
| `/gamemode`, mode menus/reset setup | Player/mode flags | In-memory changes; complete checkpoint at save. Reset first verifies the existing checkpoint. |
| `/time`, `/weather`, simulation time/weather | Clock and weather state | In memory; captured with the next complete checkpoint. |
| Game manual save, autosave, return-to-menu, process exit | `.vxw`, `.vxi`, `.vps`, `.vxm`, `.vtn`, `world_seed.txt`, `world.info` | Same coherent staged checkpoint; one commit at a time. |
| `Game::saveWorldMetadata` | `world_seed.txt`, `world.info` | Checkpoint-only serializer; no mode-change direct call remains. |
| World-selection cheats | `world.info` | `WorldLibrary::setCheats`: reserve, fresh read, metadata checkpoint. |
| Other `WorldLibrary::writeMetadata` callers | `world.info` | Existing caller's checkpoint or its own reserved checkpoint. |
| World creation | Initial `world.info` and `world_seed.txt` | Reserved coordinated transaction in a new unique directory. |
| Legacy import | Copied files plus synthesized metadata | Originals copied and checked; private-copy metadata checkpoint before folder rename. |
| UserData initial legacy copying | Existing legacy profile files | Only into an empty destination; original files untouched. Explicit test-profile overrides disable import. Subsequent loading verifies manifests/codecs. |
| Recovery | Verified previous-generation components | Explicit separate directory; rename is another checkpoint. Originals unchanged. |
| Component codec APIs | World/player/inventory/mob/navigation files | Backend refuses uncoordinated writes where a manifest exists. Pre-manifest fixture/legacy serializers remain supported. |
| Reproduction/validation code | Deliberately malformed fixture bytes | Only new temporary profiles; raw writes are explicitly marked fault injection. |

`world_seed.txt` has no remaining production raw-stream writer. The sole ordinary
Game writer is checkpoint capture; creation and import use coordinated serializers.
Global preferences, logs and screenshots are not world-checkpoint components.

## `.session.lock` investigation

The reproduced same-process conflict came from the erroneous direct metadata path,
not a stale lock file. Removing that path removes the self-conflict. Session
reservations are still enforced with an open OS handle and expire when the process
releases it/exits. An empty `.session.lock` file remaining on disk does not itself
mean the world is locked. Deleting it is not the recovery procedure.

Diagnostics now distinguish Windows sharing violation 32 from access/filesystem
errors, show the native message and requester PID, and explain that another handle
may belong to the same or another process. Tests intentionally attempt conflicting
sessions and expect refusal. The tests also cover an actual second process. No
claim is made that every prior lock message had the same cause.

## Safe recovery for affected worlds

1. Close game instances using the affected world. Back up the **entire world folder**,
   including manifests, journals and `.checkpoint-old` files. Do not edit/delete them.
2. Start v2.14.3. In Play, select the unavailable world and read the named component
   error. Press **R twice within five seconds** to request a separate recovered world.
3. Recovery verifies every previous-generation component against the previous
   manifest before copying. A `Recovered ...` world is separate from the original.
   Inspect it and its progress before deciding which world to continue.
4. If no complete verified previous generation exists, recovery refuses. Do not
   replace the manifest with hashes of the damaged files. Restore a complete known
   good external backup into a separate world instead. Some progress after the last
   complete checkpoint may not be recoverable.

This release never repairs or overwrites an affected original automatically. Tests
fingerprint every source-world file before and after recovery, reject missing or
invalid backups, and exercise the recovered copy through normal Game loading.

## Regression evidence

The retained [evidence index](checkpoint-integrity/README.md) and
[summary](checkpoint-integrity/validation.json) identify exact final packages and results.
Tests run under isolated temporary/workspace profiles; no real user saves were opened
writable, recovered or migrated. The tracked regression runner derives its build/demo
version from CMake, so it cannot accidentally run the old v2.14.2 demo.

**Final result:** both Release variants built; all **20 full/demo suites passed**.
The focused metadata driver passed **80 assertions**, including repeated refused
save attempts. Both variants match all **663** historical generator chunk records.
Both actual playback logs contain the exact **24-file** source playlist, with no
missing/extra filenames; the generated embedded-resource lists also match it.
Nine demo profiles were checked for forbidden world/navigation/checkpoint files.
The v2.14.3 setup and single-file demo checksums are retained with the evidence.

- Save, switch Creative/Survival/Spectator, change time/weather, autosave, shutdown
  and reload through actual Game/command paths. Metadata stays unchanged on disk
  until commit, and each completed manifest verifies.
- Change mode/time/weather with a background commit pending; queue another save;
  verify the latest requested coherent state survives reload.
- Cheats On/Off persists with command-executor enforcement. Stale menu metadata
  does not replace fresh fields. Active-session permission changes are refused.
- Guard every managed component, not just `world_seed.txt`, against standalone
  writes; verify source bytes remain unchanged.
- Inject v2.14.2-style metadata corruption before a worker verifies its snapshot;
  classify the component as permanent. Twenty repeated save attempts plus an
  expired autosave deadline do not rearm retries or change source files.
- Child-process interruption after data/metadata replacements but before the new
  manifest; restart rolls back a complete verified generation and retains backups.
- Locked replacement failure, 400 concurrent standalone record writes and 200
  paired checkpoints per persistence stress suite, plus immutable background capture.
- Explicit verified recovery to a copy, refusal of absent/corrupt backups, and a
  pre-manifest compatibility fixture made from a verified copied save.
- Full/demo regressions include old inventory formats, equipment, command permissions,
  fluids, weather, navigation, reset, UI, physics and historical chunk hashes.
- Music uses all files in `sounds/background`. The playback check seeks to each
  song's end to exercise actual decoder/playback transitions across the entire
  shuffle bag; the logged filename set must equal the source folder's filename set.
  A mute/no-device shortcut alone is not accepted as proof: the focused test requires
  an actual current song. This is not a full-duration listening review of each track.

Reproduction commands (fresh isolated fixtures; Windows Release toolchain):

```powershell
./package_release.ps1
./tools/run_stability_regressions.ps1 -SkipBuild -IncludeDemo
./build/v2143-validation/driver/Release/StabilityDriver.exe --checkpoint
```

## Scope and limits

- v2.14.2 worlds already damaged by the old writer are refused, not silently healed.
  Recovery requires a complete verified checkpoint; lost later progress is not invented.
- The original separate intermittent native Windows replacement failure's root cause
  remains unresolved. Existing lock/retry/rollback safeguards are retained and tested.
- Save capture/serialization remains synchronous. No general performance speedup,
  physical power-loss certification or multi-hour/physical low-end soak is claimed.
- The previously requested now-playing label and updated sound assets are retained.
  No new rendering, terrain, billboard-mob, gameplay or generator feature was added.
- Standalone remains stateless with cheats disabled. Both output filenames/version
  resources are **v2.14.3**. See the release notes for final package validation scope.
