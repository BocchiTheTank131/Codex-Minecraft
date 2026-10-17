# Voxel Frontier v2.15.0 — Farming, Food & Survival

The Farming, Food & Survival Update.

## Farming

- Four-stage carrots, potatoes and beetroot; reusable pumpkin and melon stems.
- Repeat berry harvests, three-block sugar cane and restrained mushroom spread.
- Five hoe tiers, water/rain irrigation, Bone Meal and a five-state Composter.
- Renewable seed acquisition from existing grass and ferns, including old worlds.
- Crops use chunk indexes and bounded scheduled work instead of volume scans.

## Livestock and food

- Original voxel chickens with chicks, wings, seeds, eggs, feathers and meat.
- Correct livestock foods, love timers, five-minute breeding cooldowns and
  intentionally persistent parents/offspring.
- Durable shears, wool regrowth and cooldown-based cow milking.
- New meals, cooking recipes and container-safe consumption, including offhand.
- Hunger/saturation progression; mode changes preserve valid Survival statistics.

## Compatibility

32 item IDs and 31 block IDs are appended within existing byte registries.
Validated optional nutrition and husbandry extensions remain inside the existing
checkpoint components. Missing old extensions receive safe defaults; invalid
extensions refuse loading without partial mutation. Generator V22 remains current;
there is no new terrain generator. The four billboard hostiles are retained.

All 24 background songs, Now Playing, scene-only antialiasing and v2.14.3 settings
defaults remain. The standalone demo remains stateless with cheats disabled.

Cake, poisonous potatoes, trampling and water/lava bucket transport are deferred.
The Empty Bucket is currently a milk container; it does not place fluids. Milk
provides a small renewable food source and returns its bucket.

## Downloads

- `dist/VoxelFrontier-v2.15.0-Windows-Setup.exe`: full persistent game installer.
- `dist/VoxelFrontier-v2.15.0-Windows-Standalone.exe`: stateless single-file demo.
- `dist/SHA256SUMS.txt`: package hashes.

Validation: 257 farming assertions, 20 Release suites, and 663 exact historical chunks per variant passed. Both Release packages and an isolated installer upgrade were tested.

RD8 performance was comparable; short foreground samples showed approximately 2.6% lower FPS at RD16 and 0.9% lower at RD32. Terrain GPU memory was unchanged. These measurements do not establish a rendering speedup or certify low-end hardware.

See [validation evidence and remaining limits](https://github.com/BocchiTheTank131/Codex-Minecraft/blob/v2.15.0/docs/FARMING_SURVIVAL_VALIDATION.md).
