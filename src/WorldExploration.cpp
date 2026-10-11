#include "World.h"
#include <unordered_map>

bool World::explorationStructureReservedAt(int x,int z) const {
    struct Cache {
        std::uint32_t seed=0,version=0;
        std::unordered_map<std::int64_t,std::vector<StructurePlan>> plans;
    };
    thread_local Cache cache;
    if(cache.seed!=seed_ || cache.version!=generationVersion_ || cache.plans.size()>128) {
        cache.plans.clear();cache.seed=seed_;cache.version=generationVersion_;
    }
    for(int cz=floorDiv(z-8,16);cz<=floorDiv(z+8,16);++cz)
    for(int cx=floorDiv(x-8,16);cx<=floorDiv(x+8,16);++cx) {
        const auto key=chunkKey(cx,cz);
        auto found=cache.plans.find(key);
        if(found==cache.plans.end()) found=cache.plans.emplace(key,
            structures_.plansForChunk(cx,cz,[&](int wx,int wz){return structureTerrainAt(wx,wz);})).first;
        for(const auto& plan:found->second)
            if(x>=plan.bounds.minX-8&&x<=plan.bounds.maxX+8&&z>=plan.bounds.minZ-8&&z<=plan.bounds.maxZ+8) return true;
    }
    return false;
}
