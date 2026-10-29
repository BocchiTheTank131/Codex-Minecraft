#include "World.h"
#include "TerrainDensity.h"
#include "TerrainAesthetics.h"
#include "TerrainExpansion.h"
#include <algorithm>

namespace {
float smooth(float a,float b,float v){float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
int divFloor(int n,int d){int q=n/d;return q-(n%d<0);}
}
bool World::dryUndergroundCave(int x,int y,int z) const {
    if(y<-49||y>100)return false;
    const auto t=sampleTerrain(x,z);
    if(y>=t.height-10)return false;
    const float a=aquiferNoise_.fractal2D(x*.0018f+113.7f,z*.0018f-67.3f,2,2.f,.3f);
    const float level=generationVersion_>=23?TerrainExpansion::aquiferLevel(a):-24.f+a*30.f;
    if(a>.20f&&y<=level+3.f)return false;
    const TerrainDensity::Column c{t.height,smooth(95.f,180.f,float(t.height))*(1-smooth(.40f,.83f,t.erosion)),a,level,t.ravine,
        generationVersion_>=23?TerrainExpansion::shaftAt(x,z,noodleNoise_,ridgeNoise_):-1.f};
    const float slope=float(std::max({std::abs(sampleTerrain(x-4,z).height-t.height),
        std::abs(sampleTerrain(x+4,z).height-t.height),std::abs(sampleTerrain(x,z-4).height-t.height),
        std::abs(sampleTerrain(x,z+4).height-t.height),std::abs(sampleTerrain(x-1,z).height-t.height),
        std::abs(sampleTerrain(x+1,z).height-t.height),std::abs(sampleTerrain(x,z-1).height-t.height),
        std::abs(sampleTerrain(x,z+1).height-t.height)}))*.25f;
    // Use the exact existing cave lattice and slope mask, independent of loaded
    // chunks, edits and worker scheduling.
    const auto solid=[&](int yy) {
        const auto f=generationVersion_>=23?
            TerrainDensity::sampleAtCached(x,yy,z,seed_,generationVersion_,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_):
            TerrainDensity::sampleAt(x,yy,z,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_);
        return (generationVersion_>=23?TerrainExpansion::density(f,c,yy,slope):TerrainAesthetics::density(f,c,yy,slope))>0;
    };
    return solid(y-1)&&!solid(y)&&!solid(y+1)&&!solid(y+2);
}
bool World::undergroundReserved(const StructureBox& box) const {
    const auto terrain=[&](int x,int z){return structureTerrainAt(x,z);};
    // Only during a cached regional plan build, never inside voxel traversal.
    for(int z=divFloor(box.minZ,16);z<=divFloor(box.maxZ,16);++z)
        for(int x=divFloor(box.minX,16);x<=divFloor(box.maxX,16);++x)
            for(const auto& p:structures_.plansForChunk(x,z,terrain))
                if(p.bounds.overlapsXZ(box,8))return true;
    return false;
}
std::shared_ptr<const UndergroundPlan> World::undergroundPlan(int rx,int rz) const {
    return (generationVersion_>=24?underground24_:generationVersion_>=23?underground23_:generationVersion_>=21?underground21_:generationVersion_>=20?underground20_:underground_).plan(rx,rz,[&](int x,int z){const auto t=sampleTerrain(x,z);return UndergroundSite{t.height,t.biome,t.river};},
        [&](int x,int y,int z){return dryUndergroundCave(x,y,z);},[&](const StructureBox& b){return undergroundReserved(b);});
}
bool World::applyUnderground(int cx,int cz,GeneratedChunk& data) const {
    return (generationVersion_>=24?underground24_:generationVersion_>=23?underground23_:generationVersion_>=21?underground21_:generationVersion_>=20?underground20_:underground_).apply(cx,cz,data.blocks,data.loot,data.mobs,
        [&](int x,int z){const auto t=sampleTerrain(x,z);return UndergroundSite{t.height,t.biome,t.river};},
        [&](int x,int y,int z){return dryUndergroundCave(x,y,z);},[&](const StructureBox& b){return undergroundReserved(b);},minimumY(),maximumY());
}
