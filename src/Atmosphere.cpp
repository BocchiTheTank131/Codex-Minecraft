#include "Renderer.h"
#include "World.h"
#include "Building.h"
#include <cmath>

namespace {
unsigned visualHash(unsigned x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^(x>>16);}
bool haloSource(Block b){
    auto d=buildingDefinition(b);
    return b==Block::Torch||(d&&(d->shape==BuildingShape::Lantern||d->shape==BuildingShape::Campfire)&&buildingEmission(b)>0);
}
}
std::size_t Renderer::cosmeticParticleCount()const{
    return std::count_if(particles_.begin(),particles_.end(),[](const auto& p){return p.cosmetic!=0;});
}
void Renderer::updateAtmospherics(float dt,const World& world,glm::vec3 camera,float worldTime){
    // Disabled frames do not repeatedly walk the existing combat-particle batch.
    if(effectQuality_==0||particlePercent_==0){
        if(ambientTimer_<0)return;ambientTimer_=-1;
        haloSources_.clear();particles_.erase(std::remove_if(particles_.begin(),particles_.end(),[](const auto& p){return p.cosmetic!=0;}),particles_.end());return;
    }
    ambientTimer_-=std::min(.25f,std::max(0.f,dt));if(ambientTimer_>0)return;ambientTimer_=.2f;
    const int cap=(effectQuality_==1?24:64)*particlePercent_/100;
    int retained=0;particles_.erase(std::remove_if(particles_.begin(),particles_.end(),[&](const auto& p){return p.cosmetic&&++retained>cap;}),particles_.end());
    const float day=glm::smoothstep(-.18f,.22f,glm::normalize(glm::vec3(std::cos(WorldTime::angle(worldTime))*.72f,std::sin(WorldTime::angle(worldTime)),-.42f)).y);
    auto alive=cosmeticParticleCount();
    for(int n=0;n<(effectQuality_==1?1:3)&&alive<std::size_t(cap)&&particles_.size()<4096;++n){
        unsigned h=visualHash(++ambientSequence_);glm::vec3 p=camera+glm::vec3(int(h&31)-16,int((h>>5)&15)-5,int((h>>9)&31)-16)*.4f;
        auto cell=glm::ivec3(glm::floor(p));if(!world.hasLoadedChunkAt(cell.x,cell.z)||world.getBlock(cell.x,cell.y,cell.z)!=Block::Air)continue;
        const float sky=world.sunlightAt(cell.x,cell.y,cell.z)/15.f,block=world.blockLightAt(cell.x,cell.y,cell.z)/15.f;
        if(std::max(sky,block)<.5f)continue;
        particles_.push_back({p,{.025f,-.025f,.018f},glm::vec3(.55f,.48f,.31f)*std::max(sky*day,block),3.0f,1});++alive;
    }
    const int haloCap=effectQuality_==1?4:16;
    haloSources_.erase(std::remove_if(haloSources_.begin(),haloSources_.end(),[&](auto p){return glm::distance(glm::vec3(p),camera)>12||!world.hasLoadedChunkAt(p.x,p.z)||!haloSource(world.getBlock(p.x,p.y,p.z));}),haloSources_.end());
    if(haloSources_.size()>std::size_t(haloCap))haloSources_.resize(haloCap);
    // A rotating local probe budget discovers sources; no global light scan.
    for(int n=0;n<32;++n){unsigned cell=haloScan_++%256;auto p=glm::ivec3(glm::floor(camera))+glm::ivec3(int(cell%8)-4,int(cell/64)-3,int((cell/8)%8)-4);
        if(!world.hasLoadedChunkAt(p.x,p.z)||haloSources_.size()>=std::size_t(haloCap))continue;
        if(!haloSource(world.getBlock(p.x,p.y,p.z)))continue;
        if(std::find(haloSources_.begin(),haloSources_.end(),p)==haloSources_.end())haloSources_.push_back(p);
    }
}
