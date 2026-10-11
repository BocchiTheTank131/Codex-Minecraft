#pragma once
#include "Noise.h"
#include "Block.h"
#include <array>

namespace TerrainDensity {
constexpr int MinimumY = -64, MaximumY = 320, SeaLevel = 64;
// Global lattice: neighboring chunks evaluate identical shared nodes. The
// one-cell halo covers exposure tests without consulting loaded-world state.
struct Fields { float surface, chamber, tunnelA, tunnelB, pillar, vein; };
// Sparse, exact lattice sampling for bounded V12 entrance planning; terrain
// generation continues using the cached Grid rather than per-block noise.
Fields sampleAt(int x,int y,int z,const PerlinNoise& surface,const PerlinNoise& chamber,
    const PerlinNoise& tunnels,const PerlinNoise& pillars,const PerlinNoise& veins);
class Grid {
public:
    Grid(int chunkX, int chunkZ, const PerlinNoise& surface, const PerlinNoise& chamber,
         const PerlinNoise& tunnels, const PerlinNoise& pillars, const PerlinNoise& veins, int maximumY = MaximumY);
    Fields at(int x, int y, int z) const;
private:
    static constexpr int Horizontal = 7, Vertical = 49;
    int originX_, originZ_;
    std::array<Fields, Horizontal * Horizontal * Vertical> nodes_{};
};
struct Column {
    int height;
    float mountain, aquifer, aquiferLevel, ravine;
};
float density(const Fields& fields, const Column& column, int y);
enum class OreCurve { Triangle, Uniform, Depth };
struct OreDistribution {
    Block stone, deep;
    OreCurve curve;
    float minimum, peak, maximum, probability;
};
const std::array<OreDistribution, 7>& oreDistributions();
float oreWeight(const OreDistribution& ore, float y);
std::uint32_t hash(int x, int y, int z, std::uint32_t seed);
}
