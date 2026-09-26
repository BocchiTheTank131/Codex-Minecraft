#include "Farming.h"
#include "World.h"
#include <algorithm>
#include <cmath>
std::int64_t FarmingSystem::key(const glm::ivec3&p){return ((std::int64_t)(p.x&0x1fffff)<<42)^((std::int64_t)(p.y&0x3ff)<<32)^(std::uint32_t)p.z;}
void FarmingSystem::plant(World&w,const glm::ivec3&p){if(w.getBlock(p.x,p.y,p.z)==Block::Air&&w.getBlock(p.x,p.y-1,p.z)==Block::Farmland){w.setBlock(p.x,p.y,p.z,Block::Crop0);growth_[key(p)]=0;}}
void FarmingSystem::update(float dt,World&w,const glm::vec3&player){
 scanTimer_-=dt;if(scanTimer_<=0){scanTimer_=5;int px=(int)std::floor(player.x),py=(int)std::floor(player.y),pz=(int)std::floor(player.z);for(int z=pz-32;z<=pz+32;++z)for(int x=px-32;x<=px+32;++x)for(int y=std::max(1,py-10);y<std::min(WORLD_HEIGHT-1,py+8);++y){Block b=w.getBlock(x,y,z);if(isCrop(b))growth_.try_emplace(key({x,y,z}),0.f);}}
 for(auto it=growth_.begin();it!=growth_.end();){std::int64_t k=it->first;int x=(int)((k>>42)&0x1fffff),y=(int)((k>>32)&0x3ff),z=(int)(std::uint32_t)k;if(x&0x100000)x|=~0x1fffff;Block b=w.getBlock(x,y,z);if(!isCrop(b)){it=growth_.erase(it);continue;}it->second+=dt;if(it->second>=18.f&&b!=Block::Crop3){it->second=0;w.setBlock(x,y,z,(Block)((int)b+1));}++it;}
}
