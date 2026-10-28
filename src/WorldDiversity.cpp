#include "World.h"
#include "TerrainDensity.h"
#include "TerrainAesthetics.h"
#include "TerrainExpansion.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace {
float smooth(float a,float b,float value) {
    const float t=std::clamp((value-a)/(b-a),0.f,1.f);return t*t*(3-2*t);
}
}
bool World::nativeCavityAt(int x,int y,int z) const {
    if(y<minimumY()||y>=maximumY())return false;
    struct NativeColumn {
        TerrainDensity::Column column{};float slope=0;
        std::array<std::int8_t,384> cells;
        NativeColumn(){cells.fill(-1);}
    };
    struct Cache {std::uint32_t seed=0,version=0;std::unordered_map<std::int64_t,NativeColumn> columns;};
    thread_local Cache cache;
    if(cache.seed!=seed_||cache.version!=generationVersion_) {
        cache.columns.clear();cache.columns.reserve(512);cache.seed=seed_;cache.version=generationVersion_;
    }
    // Floor/clearance probes revisit the same column at many depths. Reuse its
    // climate, aquifer, slope and exact density decisions. Each worker owns a
    // bounded cache; these procedural values never depend on loaded edits.
    const auto key=chunkKey(x,z);auto found=cache.columns.find(key);
    if(found==cache.columns.end()) {
        if(cache.columns.size()>=512)cache.columns.clear();
        NativeColumn value;
        const auto t=sampleTerrain(x,z);
        const float aquifer=aquiferNoise_.fractal2D(x*.0018f+113.7f,z*.0018f-67.3f,2,2.f,.3f);
        value.column={t.height,smooth(95,180,float(t.height))*(1-smooth(.40f,.83f,t.erosion)),
            aquifer,generationVersion_>=23?TerrainExpansion::aquiferLevel(aquifer):-24.f+aquifer*30.f,t.ravine,
            generationVersion_>=23?TerrainExpansion::shaftAt(x,z,noodleNoise_,ridgeNoise_):-1.f};
        value.slope=float(std::max({std::abs(terrainHeight(x-4,z)-t.height),std::abs(terrainHeight(x+4,z)-t.height),
            std::abs(terrainHeight(x,z-4)-t.height),std::abs(terrainHeight(x,z+4)-t.height),
            std::abs(terrainHeight(x-1,z)-t.height),std::abs(terrainHeight(x+1,z)-t.height),
            std::abs(terrainHeight(x,z-1)-t.height),std::abs(terrainHeight(x,z+1)-t.height)}))*.25f;
        found=cache.columns.emplace(key,std::move(value)).first;
    }
    auto& value=found->second;auto& cell=value.cells[y-minimumY()];
    if(cell>=0)return cell!=0;
    const auto f=generationVersion_>=23?
        TerrainDensity::sampleAtCached(x,y,z,seed_,generationVersion_,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_):
        TerrainDensity::sampleAt(x,y,z,ridgeNoise_,cheeseNoise_,spaghettiNoise_,noodleNoise_,caveNoise_);
    cell=(generationVersion_>=23?TerrainExpansion::density(f,value.column,y,value.slope):
        TerrainAesthetics::density(f,value.column,y,value.slope))<=0?1:0;
    return cell!=0;
}
std::vector<TerrainExploration::CaveEntrance> World::diversityEntrances(int cx,int cz) const {
    return TerrainExploration::caveEntrances(cx,cz,seed_,[&](int x,int z) {
        const auto t=sampleTerrain(x,z);return TerrainExploration::SurfaceColumn{t.height,t.river,t.ravine};
    },[&](int x,int y,int z){return nativeCavityAt(x,y,z);},
      [&](int x,int z){return explorationStructureReservedAt(x,z);},generationVersion_);
}
glm::vec3 World::findSurvivalSpawnNear(int originX,int originZ) const {
    if(generationVersion_<22) return findSafeSpawnNear(originX,originZ);
    if(originX==0 && originZ==0 && survivalSpawnRegion_) {
        try {return findCollisionSpawnNear(survivalSpawnRegion_->x,survivalSpawnRegion_->y);}
        catch(const std::runtime_error&){survivalSpawnRegion_.reset();}
    }
    struct Candidate {int x,z; float score;};
    std::vector<Candidate> candidates;candidates.reserve(1297);
    // 289 deterministic coarse candidates plus at most 1008 fallback candidates.
    // Geography is not modified to suit any of them. Fine block safety is
    // still checked by the established collision-volume spawn routine.
    const auto consider=[&](int dx,int dz) {
        const int x=originX+dx*256,z=originZ+dz*256;
        const auto t=sampleTerrain(x,z);
        if(t.height<72 || t.height>170 || t.river>.01f || t.ravine>.02f ||
           t.continentalness<.035f) return;
        int land=0,resources=0,relief=0;
        std::set<Biome> habitats;
        for(int ring:{128,384,768})for(const auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
            const auto nearby=sampleTerrain(x+d.x*ring,z+d.y*ring);
            if(nearby.height<68 || nearby.river>.6f) continue;
            ++land;habitats.insert(nearby.biome);
            resources+=biomeDefinition(nearby.biome).trees!=TreeStyle::None;
            if(ring==128)relief+=std::abs(nearby.height-t.height);
        }
        if(land<9 || !resources) return;
        const int localSlope=std::max({std::abs(terrainHeight(x-8,z)-t.height),std::abs(terrainHeight(x+8,z)-t.height),
            std::abs(terrainHeight(x,z-8)-t.height),std::abs(terrainHeight(x,z+8)-t.height)});
        if(localSlope>5) return;
        const float score=float(habitats.size())*18.f+land*4.f+std::min(resources,4)*3.f-
            localSlope*3.f-relief*.10f-std::hypot(float(dx),float(dz))*1.5f;
        candidates.push_back({x,z,score});
    };
    for(int dz=-8;dz<=8;++dz)for(int dx=-8;dx<=8;++dx)consider(dx,dz);
    if(candidates.empty()) {
        // Rare ocean-heavy seeds search a second, coarser bounded annulus,
        // rather than accepting the first tiny island near the origin.
        for(int dz=-16;dz<=16;++dz)for(int dx=-16;dx<=16;++dx)
            if(std::max(std::abs(dx),std::abs(dz))>4)consider(dx*2,dz*2);
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){return a.score>b.score;});
    // Only the best eight regions pay native cave and exact collision costs.
    glm::vec3 best{};float bestScore=-1e9f;bool found=false;
    for(std::size_t i=0;i<std::min<std::size_t>(8,candidates.size());++i) {
        const auto candidate=candidates[i];
        glm::vec3 safe;
        try {safe=findCollisionSpawnNear(candidate.x,candidate.z);}catch(const std::runtime_error&){continue;}
        float nearest=800;
        for(int dz=-480;dz<=480;dz+=160)for(int dx=-480;dx<=480;dx+=160) {
            if(std::hypot(float(dx),float(dz))>640) continue;
            for(const auto& cave:diversityEntrances(floorDiv(int(safe.x)+dx,16),floorDiv(int(safe.z)+dz,16))) {
                const float distance=std::hypot(cave.mouth.x-safe.x,cave.mouth.z-safe.z);
                nearest=std::min(nearest,distance);
            }
        }
        const float score=candidate.score+std::max(0.f,600-nearest)*.04f;
        if(!found || score>bestScore) {found=true;best=safe;bestScore=score;}
        // The candidates are already ranked for land/resources/variety. A
        // good nearby cave completes the goal; do not scan seven more regions.
        if(nearest<=500.f) {best=safe;break;}
    }
    if(found) {
        if(originX==0 && originZ==0)survivalSpawnRegion_=glm::ivec2(int(std::floor(best.x)),int(std::floor(best.z)));
        return best;
    }
    // Ocean-heavy seeds still receive a finite fallback search. No artificial
    // island, unsafe airborne position, or modification of existing saves.
    const auto dryLand=[&](int x,int z) {
        const auto t=sampleTerrain(x,z);
        return t.height>=seaLevel()+5 && t.biome!=Biome::River && t.biome!=Biome::FrozenRiver;
    };
    for(int ring=0;ring<=16;++ring)for(int dz=-ring;dz<=ring;++dz)for(int dx=-ring;dx<=ring;++dx) {
        if(std::max(std::abs(dx),std::abs(dz))!=ring)continue;
        const int x=originX+dx*512,z=originZ+dz*512;
        if(!dryLand(x,z))continue;
        int footprint=0;
        for(int distance:{128,384,768})for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)})
            footprint+=dryLand(x+d.x*distance,z+d.y*distance);
        if(footprint<9)continue;
        try {
            const auto safe=findCollisionSpawnNear(x,z);
            if(originX==0 && originZ==0)survivalSpawnRegion_=glm::ivec2(int(std::floor(safe.x)),int(std::floor(safe.z)));
            return safe;
        }catch(const std::runtime_error&){}
    }
    throw std::runtime_error("No safe natural land within bounded V22 spawn search");
}
