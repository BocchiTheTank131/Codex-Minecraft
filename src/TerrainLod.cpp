#include "World.h"
#include "TerrainLod.h"

void World::updateTerrainLod(const glm::vec3& camera) {
    lodCamera_=camera;
    if(terrainLod_==0 && lodDisableRemaining_==0)return;
    const int cx=floorDiv(int(std::floor(camera.x)),CHUNK_SIZE),cz=floorDiv(int(std::floor(camera.z)),CHUNK_SIZE);
    const auto update=[&](Chunk& chunk,bool forceNear=false) {
        const float dx=chunk.x*16+8.f-camera.x,dz=chunk.z*16+8.f-camera.z;
        const int desired=forceNear?0:TerrainLod::desired(std::sqrt(dx*dx+dz*dz),renderDistance_,terrainLod_,chunk.requestedLod);
        if(chunk.requestedLod==desired)return;
        chunk.requestedLod=desired;
        markDirty(chunk.x,chunk.z,desired==0);
    };
    // Interactable chunks recover full detail immediately after teleportation.
    if(lodNearCenter_!=glm::ivec2(cx,cz)) {
        lodNearCenter_={cx,cz};
        for(int z=-3;z<=3;++z)for(int x=-3;x<=3;++x)if(auto* chunk=findChunk(cx+x,cz+z))update(*chunk,true);
    }
    if(chunks_.empty())return;
    auto it=chunks_.find(lodCursor_);if(it==chunks_.end())it=chunks_.begin();
    // Bounded amortized work; no per-frame traversal of every loaded chunk.
    for(std::size_t i=0;i<std::min<std::size_t>(64,chunks_.size());++i) {
        update(*it->second);++it;if(it==chunks_.end())it=chunks_.begin();
    }
    lodCursor_=it->first;
    if(terrainLod_==0)lodDisableRemaining_-=std::min(lodDisableRemaining_,std::min<std::size_t>(64,chunks_.size()));
}
void World::setTerrainLod(int strength) {
    strength=std::clamp(strength,0,3);if(strength==terrainLod_)return;
    lodDisableRemaining_=strength==0?chunks_.size():0;
    terrainLod_=strength;lodNearCenter_={INT_MAX,INT_MAX};
    // A user toggle is rare. Restore all requested levels now; the actual work
    // remains asynchronous and budgeted. A changing hash-map traversal during
    // streaming must not leave any old far-detail request behind after Off.
    if(strength==0) {
        for(auto& entry:chunks_)if(entry.second->requestedLod) {
            entry.second->requestedLod=0;markDirty(entry.second->x,entry.second->z,true);
        }
        lodDisableRemaining_=0;
    }
}
