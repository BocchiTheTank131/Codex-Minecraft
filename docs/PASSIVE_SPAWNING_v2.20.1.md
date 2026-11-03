# v2.20.1 passive wildlife spawning

User-authorized balance follow-up on the uncommitted village/livestock candidate.
Existing work, installations and real saved worlds were preserved. Nothing was
committed, pushed, tagged or published.

`src/Survival.cpp`: passive cooldown **20 → 10 seconds**, passive population
ceiling **26 → 52**. The existing five-second spawning update and maximum three
new passives per batch remain. This doubles the sustained eligible spawn rate
and ceiling without increasing the per-update candidate budget.

Pigs, cows, sheep and chickens retain their habitat, safety and population
history rules. Villages, breeding and already existing animals are not duplicated.
Actual encounters depend on suitable loaded terrain and the existing per-chunk
spawn history. The passive ceiling also counts existing passive NPCs as before.

Hostile cap 18, base candidates 16, nighttime bonus 8, 0.52 hostile probability,
species weights and hostile behavior/art are unchanged. RNG is shared with the
existing spawning loop, so exact future hostile positions are not guaranteed
identical when passive admissions change; hostile spawning settings are preserved.

Both Windows Release packages were rebuilt locally without running the installer.
The existing farming regression and full/demo Survival smoke tests passed in
fresh marked profiles. Demo output was checked for absence of persistent saves.
Broad generation/performance matrices were not repeated for these two settings.

[Results](passive-spawn-v2201/results.json), [logs](passive-spawn-v2201/logs.zip),
[checksums](passive-spawn-v2201/SHA256SUMS.txt).
These packages supersede the earlier v2.20.1 livestock-stage binary fingerprints.

- `VoxelFrontier-v2.20.1-Windows-Setup.exe`: SHA-256 `5f1eff2edae31c079fe7fed6297bcad1d203008e9b9f5d1cd948e9687b9d01e1`.

- `VoxelFrontier-v2.20.1-Windows-Standalone.exe`: SHA-256 `475c729faac74d6df4f134592ad3c5dd25a7e4b222017d5c728175fce53fa3a1`.
