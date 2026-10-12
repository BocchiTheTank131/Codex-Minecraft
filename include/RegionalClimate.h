#pragma once

class PerlinNoise;

// V16 climate only. Terrain retains its V15 thermal inputs; callers apply
// this field after height shaping, before biome and weather classification.
namespace RegionalClimate {
float temperature(const PerlinNoise& noise, int x, int z,
                  float macro, float thermal, float habitat);
}
