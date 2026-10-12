#pragma once

// V15 surface shaping only. Underground density and legacy generators keep
// their existing solvers. All inputs are cached column samples, not block noise.
namespace TerrainChaos {
inline constexpr int NaturalMaximumY = 257; // exclusive; edits still reach 319
float height(float continent, float erosion, float mountain, float broad,
             float rolling, float crag, float detail);
float compress(float height);
}
