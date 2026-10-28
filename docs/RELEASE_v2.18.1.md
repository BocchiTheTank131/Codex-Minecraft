# Voxel Frontier v2.18.1 — Engine Optimization Update

Windows release: full persistent-game installer and stateless standalone demo.

- Reuse eligible shared terrain pages and amortize safe GPU object retirement
  above RD32; retain the proven cleanup policy at normal render distances. Improve diagnostics for driver deletion, allocation and uploads.
- Reuse exact immutable Generator V23 column results without changing terrain,
  caves, vegetation, structures or loot.
- Move autosave verification, encoding and staging to the checkpoint worker using
  coherent immutable snapshots. Preserve existing transaction/recovery guarantees.
- Coalesce repeated high-priority mesh requests while retaining every revision.
- Respect available process CPUs when sizing streaming workers.

All generators V1–V23, 361 item previews, IDs, saves, automation, gameplay and the
four hostile billboard mobs are preserved. No new generation or save format.

See [validation](ENGINE_OPTIMIZATION_VALIDATION_v2.18.1.md) for measurements,
remaining RD64/driver limitations, regression coverage and local packages.

Validation: 20 Release suites, 1,326 historical hashes and 495 exact V23 fixtures per variant passed. Matched tests measured about 20% lower cold generation time and 250k-edit save capture of 14.59 -> 5.19 ms. RD64 driver/render stalls remain; no universal FPS improvement is claimed.
