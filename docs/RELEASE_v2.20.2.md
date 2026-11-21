# v2.20.2 — Visual & Atmospheric Polish

Release authorized November 21, 2026.

Download the **Windows Setup** for the full persistent game or **Windows Standalone**
for the stateless demo from the [v2.20.2 release](https://github.com/BocchiTheTank131/Codex-Minecraft/releases/tag/v2.20.2).
`SHA256SUMS.txt` verifies both downloads. No installation or real-save changes
were performed during preparation.

- Richer blue sky gradients, directional warm twilight and cool nights; bounded
  procedural cloud detail, softer edges, subdued stars and cloud-occluded sun/moon.
- Shared celestial palette and local light response across terrain, voxel models,
  world item sprites and billboards. Held objects respond to nearby light.
  Original enemy art and all mob behavior are retained.
- Animated water normals, grazing-angle sky tint and restrained solar highlights;
  clearer transparency without additional reflection or depth-buffer passes.
- Smooth underwater attenuation follows the camera crossing the actual fluid
  surface. Nearby textures remain readable; water physics are unchanged.
- Distance-scaled clear haze, separate from the existing Weather Fog preference;
  cave skylight gating, storm coloration and subtle exposed-surface wet highlights.
- More transparent rain, fading splash rings, gentle snow drift and softer lightning.
- Base-anchored grass/flower wind on Medium/High. Connected leaf cubes stay rigid
  to preserve their seams and collision silhouette.
- Sparse, bounded dust and small depth-tested warm points near verified torches,
  lit lanterns and campfires. Low disables these cosmetic additions.
- Dark outer selection borders improve inventory/hotbar visibility against bright
  scenery. Existing icons, counts, dragging, scaling and input remain intact.

No new items, recipes, world generators, save formats or graphics settings.
All work uses GLSL 330 and the existing forward renderer. No bloom, ray marching,
extra reflection framebuffer, dynamic shadow maps or gameplay lighting is added.

Quality uses existing Effect Quality, Weather Quality and particle amount settings.
Low retains improved color and lighting with no cloud detail, wind, dust or halos.
Medium uses two cloud octaves, modest crossed-plant motion, at most 24 dust motes
and four cached light-source points. High uses four octaves, stronger but subtle
motion, at most 64 dust motes and sixteen light-source points. Particle amount
also scales dust; zero clears both new particle effects.

See [validation](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.20.2/docs/VISUAL_POLISH_VALIDATION_v2.20.2.md) for native comparisons,
measured performance, package verification, compatibility and unresolved limits.
Physical low-end/integrated-GPU certification is not implied by desktop testing.

Validation: 15 functional groups, 11 generation groups and 20 packaged full/demo
Release suites pass; 1,326 historical records, 495 V23 and 738 V24 exact fixtures
per variant match. Original 365 preview images and mappings are unchanged.

Performance limit: GPU median overhead at RD8 is about 1–3% on the tested desktop,
but active engine work rises 5.8% on Low and 9.6% on Medium, missing those targets.
Some RD8 p99 increases remain variable. See the report before judging low-end
readiness. Interior models can remain too dark near doorways. Textured drifting
leaves and deformation of connected leaf cubes remain deferred.
