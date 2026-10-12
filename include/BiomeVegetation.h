#pragma once
#include "Biome.h"
#include <functional>
#include <vector>

struct BiomeTerrain { int height; Biome biome; float river; };
struct VegetationCounts { std::size_t trees=0, bamboo=0; };
// World-coordinate decisions; each invocation writes only its own chunk.
void decorateBiomeChunk(int chunkX, int chunkZ, std::uint32_t seed,
                        std::vector<Block>& blocks,
                        const std::function<BiomeTerrain(int,int)>& terrain,
                        const std::function<bool(int,int)>& reserved,
                        const std::function<bool(int,int)>& supported,
                        int minimumY = 0, int maximumY = 256, int seaLevel = 48, unsigned generatorVersion = 11, VegetationCounts* counts = nullptr);
