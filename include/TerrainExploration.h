#pragma once
#include "Biome.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>
namespace TerrainExploration {
float heightVariation(float continentalness,float erosion,float river,float rolling,float ridge);
Biome variant(Biome base,float temperature,float humidity,float habitat,int elevation);
struct SurfaceColumn { int height; float river,ravine; };
struct CaveEntrance { glm::vec3 mouth,bend,target; float radius; bool sinkhole; bool preserveFloor=false; };
std::vector<CaveEntrance> caveEntrances(int chunkX,int chunkZ,std::uint32_t seed,
    const std::function<SurfaceColumn(int,int)>& terrain,
    const std::function<bool(int,int,int)>& cavity,
    const std::function<bool(int,int)>& reserved,unsigned generationVersion=12);
bool carves(const CaveEntrance& cave,int x,int y,int z);
std::vector<CaveEntrance> diverseCaveEntrances(int chunkX,int chunkZ,std::uint32_t seed,
    const std::function<SurfaceColumn(int,int)>& terrain,
    const std::function<bool(int,int,int)>& cavity,
    const std::function<bool(int,int)>& reserved,unsigned generationVersion=22);
bool mushroomCluster(int x,int z,std::uint32_t seed,Biome biome);
}
