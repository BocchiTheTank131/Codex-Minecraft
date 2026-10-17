# v2.14.3 additional settings/UI polish

Follow-up to the checkpoint correction; publication approved for v2.14.3.

- Removed the fourth, out-of-panel Video settings header button. The three visible
  tabs now match the existing Video/Audio/Controls hit targets. The bottom Back
  button remains.
- Fresh settings: music 50%, render distance 8, simulation distance 8, terrain LOD
  Low, occlusion Auto. F3 starts closed. Installed-game saved preferences still
  load normally; the stateless demo uses the new defaults each launch.
- MSAA renders the 3D scene to its own target and resolves before HUD, inventory,
  menus, maps, held sprites and song text. The window backbuffer requests zero MSAA
  samples. Turning AA off no longer resolves a stale multisample target.
- Underwater tint affects the scene before HUD drawing.

Both Release distributions are rebuilt as v2.14.3, with refreshed checksums.
This follow-up's validation is compilation and source/build inspection; the retained
20-suite checkpoint report and hashes describe the preceding checkpoint-correction
build, rather than a new regression run of these UI/default changes.

No world-generation, persistence transaction or billboard-mob behavior was changed.
Final distribution hashes are recorded in `v2143-release-artifacts.json`.
