#pragma once
#include "Biome.h"
#include <functional>
#include <vector>

struct BiomeTerrain { int height; Biome biome; float river; };
// World-coordinate decisions; each invocation writes only its own chunk.
void decorateBiomeChunk(int chunkX, int chunkZ, std::uint32_t seed,
                        std::vector<Block>& blocks,
                        const std::function<BiomeTerrain(int,int)>& terrain,
                        const std::function<bool(int,int)>& reserved,
                        const std::function<bool(int,int)>& supported);
