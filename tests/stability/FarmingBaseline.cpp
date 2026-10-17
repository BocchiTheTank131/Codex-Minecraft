// Exact v2.14.3 farming snapshot, class/include renamed for matched CPU tests.
#include "FarmingBaseline.h"
#include "World.h"
#include <algorithm>
#include <cmath>
void BaselineFarmingSystem::plant(World& w, const glm::ivec3& p) {
    if (w.getBlock(p.x, p.y, p.z) == Block::Air &&
        (w.getBlock(p.x, p.y - 1, p.z) == Block::Farmland || w.getBlock(p.x, p.y - 1, p.z) == Block::WetFarmland)) {
        w.setBlock(p.x, p.y, p.z, Block::Crop0);
        growth_[p] = 0;
    }
}
void BaselineFarmingSystem::update(float dt, World& w, const glm::vec3& player) {
    scanTimer_ -= dt;
    if(scanTimer_<=0 && scanCursor_==0){
        scanOrigin_=glm::ivec3(glm::floor(player));scanRadius_=std::min(48,w.simulationDistance()*CHUNK_SIZE);
        scanLow_=std::max(w.minimumY()+1,scanOrigin_.y-10);scanHigh_=std::min(w.maximumY()-1,scanOrigin_.y+8);
        scanTimer_=5;
    }
    const int side=2*scanRadius_+1,height=std::max(0,scanHigh_-scanLow_),total=side*side*height;
    if(scanCursor_<total){
        const int end=std::min(total,scanCursor_+512);
        for(;scanCursor_<end;++scanCursor_){const int column=scanCursor_/height;
            glm::ivec3 p(scanOrigin_.x-scanRadius_+column%side,scanLow_+scanCursor_%height,scanOrigin_.z-scanRadius_+column/side);
            if(isCrop(w.getBlock(p.x,p.y,p.z)))growth_.try_emplace(p,0.f);
        }
    }
    if(scanCursor_==total && scanTimer_<=0)scanCursor_=0;
    for (auto it = growth_.begin(); it != growth_.end();) {
        const auto& p=it->first;const int x=p.x,y=p.y,z=p.z;
        if(!w.hasLoadedChunkAt(x,z)){++it;continue;}
        Block b = w.getBlock(x, y, z);
        if (!isCrop(b)) {
            it = growth_.erase(it);
            continue;
        }
        if (!w.simulationActiveAt(static_cast<float>(x), static_cast<float>(z))) {
            ++it;
            continue;
        }
        it->second += dt * (w.getBlock(x,y-1,z)==Block::WetFarmland ? 1.25f : 1.0f);
        if (it->second >= 18.f && b != Block::Crop3) {
            it->second = 0;
            w.setBlock(x, y, z, (Block)((int)b + 1));
        }
        ++it;
    }
}
