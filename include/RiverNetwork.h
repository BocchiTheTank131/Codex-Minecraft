#pragma once
#include <cstdint>
#include <functional>
#include <array>
#include <vector>

namespace RiverNetwork {
struct Ground { float continent; };
struct Sample { float channel=0, valley=0, waterLevel=64; };
struct Point { float x,z; };
struct Reach {
    std::array<Point,9> p;
    float from,to,halfWidth;
    int incoming;
    float minX,maxX,minZ,maxZ;
};
// World-coordinate drainage graph. The cache is private to each calling thread
// and bounded; results depend only on seed/coordinates, never cache contents.
Sample sample(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground,float terrainHeight=64);
// Development inspection uses the same cached reaches as native generation.
std::vector<Reach> inspectCell(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground);
}
