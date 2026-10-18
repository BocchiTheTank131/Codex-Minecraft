#include "Farming.h"
#include "Building.h"
#include "World.h"
#include "Player.h"
#include "Definitions.h"
#include "Weather.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
constexpr glm::ivec3 directions[]={{1,0,0},{0,0,1},{-1,0,0},{0,0,-1}};
std::uint32_t hash(const glm::ivec3& p, std::uint32_t seed) {
    auto h=seed ^ std::uint32_t(p.x)*73856093u ^ std::uint32_t(p.y)*19349663u ^ std::uint32_t(p.z)*83492791u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;return h;
}
bool ground(Block b) {return b==Block::Grass || b==Block::Dirt || b==Block::Podzol || b==Block::CoarseDirt || b==Block::Mycelium || b==Block::Mud;}
bool loaded(const World& w,const glm::ivec3& p) {return p.y>w.minimumY() && p.y<w.maximumY() && w.hasLoadedChunkAt(p.x,p.z) && w.simulationActiveAt(float(p.x),float(p.z));}
bool neighborsLoaded(const World& w,const glm::ivec3& p,int radius) {
    for(int x:{-radius,0,radius})for(int z:{-radius,0,radius})if(!w.hasLoadedChunkAt(p.x+x,p.z+z))return false;
    return true;
}
}
Block FarmingSystem::plantedBlock(Item item) {
    switch(item) {
    case Item::Seeds:return Block::Crop0;
    case Item::Carrot:return Block::Carrot0;
    case Item::Potato:return Block::Potato0;
    case Item::BeetrootSeeds:return Block::Beetroot0;
    case Item::PumpkinSeeds:return Block::PumpkinStem0;
    case Item::MelonSeeds:return Block::MelonStem0;
    case Item::SweetBerries:return Block::BerryBush0;
    default:return Block::Air;
    }
}
bool FarmingSystem::plant(World& w,const glm::ivec3& p,Item item) {
    const auto block=plantedBlock(item);
    if(block==Block::Air || !loaded(w,p) || w.getBlock(p.x,p.y,p.z)!=Block::Air || !w.canPlacePlant(p,block))return false;
    w.setBlock(p.x,p.y,p.z,block);return w.getBlock(p.x,p.y,p.z)==block;
}
bool FarmingSystem::hydrated(const World& w,const glm::ivec3& p) {
    for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x) {
        if(!w.hasLoadedChunkAt(p.x+x,p.z+z))continue;
        for(int y=0;y<=1;++y)if(isWater(w.getBlock(p.x+x,p.y+y,p.z+z)))return true;
    }
    return false;
}
bool FarmingSystem::grow(World& w,const glm::ivec3& p,Block b,bool forced) {
    if(!loaded(w,p))return false;
    if(isCrop(b)) {
        if(!isFarmland(w.getBlock(p.x,p.y-1,p.z)))return false;
        if(!forced && std::max(w.sunlightAt(p.x,p.y,p.z),w.blockLightAt(p.x,p.y,p.z))<9)return false;
        const int stage=cropStage(b);
        if(stage<3) {w.setBlock(p.x,p.y,p.z,Block(int(b)+1));return true;}
        const Block base=cropBase(b);
        if(base!=Block::PumpkinStem0 && base!=Block::MelonStem0)return false;
        if(!neighborsLoaded(w,p,1))return false; // An unloaded neighbor may already contain this stem's fruit.
        const auto fruit=base==Block::PumpkinStem0?Block::Pumpkin:Block::Melon;
        for(auto d:directions)if(w.getBlock(p.x+d.x,p.y,p.z+d.z)==fruit)return false;
        const auto start=hash(p,w.seed())%4;
        for(int i=0;i<4;++i) {
            const auto q=p+directions[(start+i)%4];
            if(loaded(w,q) && w.getBlock(q.x,q.y,q.z)==Block::Air &&
               (ground(w.getBlock(q.x,q.y-1,q.z)) || isFarmland(w.getBlock(q.x,q.y-1,q.z)))) {
                w.setBlock(q.x,q.y,q.z,fruit);return true;
            }
        }
    } else if(isBerryBush(b) && b!=Block::BerryBush3) {
        if(!ground(w.getBlock(p.x,p.y-1,p.z)))return false;
        if(!forced && std::max(w.sunlightAt(p.x,p.y,p.z),w.blockLightAt(p.x,p.y,p.z))<8)return false;
        w.setBlock(p.x,p.y,p.z,Block(int(b)+1));return true;
    } else if(b==Block::SugarCane) {
        if(!neighborsLoaded(w,p,1))return false;
        if(w.getBlock(p.x,p.y+1,p.z)!=Block::Air)return false;
        int base=p.y;while(base>p.y-3 && w.getBlock(p.x,base-1,p.z)==Block::SugarCane)--base;
        if(p.y-base>=2 || !w.canPlacePlant({p.x,base,p.z},b) || p.y+1>=w.maximumY())return false;
        w.setBlock(p.x,p.y+1,p.z,b);return true;
    } else if(b==Block::RedMushroom || b==Block::BrownMushroom) {
        if(w.sunlightAt(p.x,p.y,p.z)>4 || w.blockLightAt(p.x,p.y,p.z)>10)return false;
        int count=0;
        for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)for(int y=-1;y<=1;++y) {
            const auto n=w.getBlock(p.x+x,p.y+y,p.z+z);
            count+=n==Block::RedMushroom || n==Block::BrownMushroom;
        }
        if(count>=4)return false;
        const auto h=hash(p,w.seed()+std::uint32_t(clock_/180));
        const auto q=p+directions[h%4];
        if(loaded(w,q) && w.getBlock(q.x,q.y,q.z)==Block::Air && ground(w.getBlock(q.x,q.y-1,q.z)) && w.sunlightAt(q.x,q.y,q.z)<=4 && w.blockLightAt(q.x,q.y,q.z)<=10) {
            w.setBlock(q.x,q.y,q.z,b);return true;
        }
    }
    return false;
}
bool FarmingSystem::fertilize(World& w,const glm::ivec3& p) {
    const auto b=w.getBlock(p.x,p.y,p.z);
    if(!isCrop(b) && !isBerryBush(b) && b!=Block::SugarCane)return false;
    // Fertilizer matures stems; only the natural timer produces their fruit.
    // Otherwise one melon -> seeds -> compost could refund more fertilizer
    // than an immediate repeated fruit operation consumed.
    if(isCrop(b) && cropStage(b)==3)return false;
    if(!grow(w,p,b,true))return false;
    if((hash(p,w.seed()+std::uint32_t(clock_))&1) && ((isCrop(b)&&cropStage(b)<2) || (isBerryBush(b)&&b<Block::BerryBush2)))grow(w,p,Block(int(b)+1),true);
    return true;
}
void FarmingSystem::update(float dt,World& w,const glm::vec3&,const Weather* weather,float daylight) {
    if(!std::isfinite(dt) || dt<=0)return;
    const auto start=std::chrono::steady_clock::now();clock_+=dt;
    if(clock_>=discoveryTime_) {
        discoveryTime_=clock_+2;w.activeFarmCells(discovery_);
        for(const auto& p:discovery_)if(states_.find(p)==states_.end()) {
            states_.emplace(p,State{clock_,0,w.getBlock(p.x,p.y,p.z),w.farmResidencyAt(p.x,p.z)});
            due_.push({clock_+double(hash(p,w.seed())%400)/100.,p});
        }
    }
    // Hard per-step ceiling; late deadlines stay queued rather than being lost.
    for(int budget=0;budget<64 && !due_.empty() && due_.top().time<=clock_;++budget) {
        const auto p=due_.top().p;due_.pop();auto it=states_.find(p);if(it==states_.end())continue;
        if(!loaded(w,p)) {states_.erase(it);continue;}
        const auto b=w.getBlock(p.x,p.y,p.z);
        if(!isFarmTracked(b)) {states_.erase(it);continue;}
        auto& state=it->second;
        const auto residency=w.farmResidencyAt(p.x,p.z);
        if(state.residency!=residency){state.growth=0;state.last=clock_;state.residency=residency;}
        if(state.block!=b) {state.growth=0;state.block=b;}
        const float elapsed=float(std::clamp(clock_-state.last,0.,4.1));state.last=clock_;
        const auto below=w.getBlock(p.x,p.y-1,p.z);
        if(isFarmland(b)) {
            bool wet=hydrated(w,p);
            if(!wet && weather && weather->intensity()>.15f && Weather::precipitation(w,p.x,p.z)==Precipitation::Rain)
                wet=Weather::exposed(w,glm::vec3(p)+glm::vec3(.5f,1,.5f));
            const bool neighbors=neighborsLoaded(w,p,4);
            if(wet && b==Block::Farmland)w.setBlock(p.x,p.y,p.z,Block::WetFarmland);
            else if(!wet && neighbors && b==Block::WetFarmland)w.setBlock(p.x,p.y,p.z,Block::Farmland);
        } else if((isCrop(b) && !isFarmland(below)) || (isBerryBush(b) && !ground(below)) ||
                  (b==Block::SugarCane && neighborsLoaded(w,p,1) && !w.canPlacePlant(p,b))) {
            w.setBlock(p.x,p.y,p.z,Block::Air);states_.erase(it);continue;
        } else {
            const bool lit=std::max(w.sunlightAt(p.x,p.y,p.z)*std::clamp(daylight,0.f,1.f),float(w.blockLightAt(p.x,p.y,p.z)))>=9;
            if(lit || b==Block::RedMushroom || b==Block::BrownMushroom)
                state.growth+=elapsed*(below==Block::WetFarmland?1.4f:1.f);
            const float target=(b==Block::RedMushroom || b==Block::BrownMushroom)?180.f:
                b==Block::SugarCane?70.f:28.f+float(hash(p,w.seed())%13);
            if(state.growth>=target) {state.growth=0;grow(w,p,b,false);}
        }
        due_.push({clock_+4,p});
    }
    updateMilliseconds_=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
}
std::vector<ItemStack> FarmingSystem::harvest(Block b,const glm::ivec3& p,std::uint32_t seed) {
    std::vector<ItemStack> out;auto add=[&](Item i,int n=1){out.push_back({i,n});};const auto h=hash(p,seed);
    if(isBuilding(b)){auto d=buildingDefinition(b);add(blockToItem(b),d->shape==BuildingShape::Slab && (blockProperties(b)&8)?2:1);return out;}
    if(isCrop(b)) {
        const auto base=cropBase(b);const bool ripe=cropStage(b)==3;
        if(base==Block::Crop0) {add(Item::Seeds,ripe?2+int(h%2):1);if(ripe)add(Item::Wheat,1+int(h%2));}
        else if(base==Block::Carrot0)add(Item::Carrot,ripe?2+int(h%3):1);
        else if(base==Block::Potato0)add(Item::Potato,ripe?2+int(h%3):1);
        else if(base==Block::Beetroot0) {add(Item::BeetrootSeeds,ripe?2:1);if(ripe)add(Item::Beetroot,1+int(h%2));}
        else add(base==Block::PumpkinStem0?Item::PumpkinSeeds:Item::MelonSeeds);
    } else if(isFarmland(b))add(Item::Dirt);
    else if(b==Block::Melon)add(Item::MelonSlice,3+int(h%3));
    else if(isBerryBush(b))add(Item::SweetBerries,b==Block::BerryBush3?2+int(h%2):1);
    else if(b==Block::TallGrass || b==Block::Fern) {
        const Item seeds[]={Item::Seeds,Item::Carrot,Item::Potato,Item::BeetrootSeeds,Item::PumpkinSeeds,Item::MelonSeeds,Item::SweetBerries,Item::SugarCane};
        if(h%8==0)add(seeds[(h/8)%8]);
    } else if(isComposter(b))add(Item::Composter);
    else {const auto i=blockToItem(b);if(i!=Item::None)add(i,blockDefinition(b).dropCount);}
    return out;
}
bool FarmingSystem::interact(World& w,Inventory& inventory,SurvivalWorld& survival,const glm::ivec3& p,
    const glm::ivec3& adjacent,const glm::ivec3& normal,Item item,bool offhand,bool creative) {
    feedback_=nullptr;
    const auto b=w.getBlock(p.x,p.y,p.z);const auto held=offhand?inventory.equipment(4):inventory.selectedStack();
    auto consume=[&]{return creative || (offhand?inventory.consumeOffhand():inventory.consumeSelected());};
    auto drop=[&](Item i,int n){survival.spawnDrop(glm::vec3(p)+glm::vec3(.5f,1,.5f),i,n);};
    if(isComposter(b)) {
        if(b==Block::Composter4) {w.setBlock(p.x,p.y,p.z,Block::Composter0);drop(Item::BoneMeal,1);return true;}
        const bool organic=itemDefinition(item).foodContainer==Item::None && (plantedBlock(item)!=Block::Air || isFood(item) || item==Item::Wheat || item==Item::SugarCane || item==Item::RedMushroom || item==Item::BrownMushroom || isLeaf(itemToBlock(item)));
        if(organic && !held.empty() && consume())w.setBlock(p.x,p.y,p.z,Block(int(b)+1));
        else feedback_="Compost spare crops, seeds or leaves.";
        return true;
    }
    if(isBerryBush(b) && b>=Block::BerryBush2 && item!=Item::BoneMeal) {
        w.setBlock(p.x,p.y,p.z,Block::BerryBush1);drop(Item::SweetBerries,b==Block::BerryBush3?3:2);return true;
    }
    if(item==Item::BoneMeal && (isCrop(b)||isBerryBush(b)||b==Block::SugarCane)) {
        if(!held.empty() && fertilize(w,p))consume();
        else feedback_="This plant needs more growing space, or is already mature.";
        return true;
    }
    if((itemDefinition(item).tool==ToolKind::Hoe || itemDefinition(item).tool==ToolKind::Shovel) && (b==Block::Dirt || b==Block::Grass || b==Block::CoarseDirt)) {
        if(normal.y>0 && w.getBlock(p.x,p.y+1,p.z)==Block::Air) {
            w.setBlock(p.x,p.y,p.z,hydrated(w,p)?Block::WetFarmland:Block::Farmland);
            if(!creative) {if(offhand)inventory.damageOffhandTool();else inventory.damageSelectedTool();}
        }
        else feedback_="Till from above, with clear space over the soil.";
        return true;
    }
    if(plantedBlock(item)!=Block::Air && (isFarmland(b) || (item==Item::SweetBerries && ground(b)))) {
        if(normal.y>0 && adjacent==p+glm::ivec3(0,1,0) && !held.empty() && plant(w,adjacent,item))consume();
        else feedback_="Plants need clear space above suitable soil.";
        return true;
    }
    return false;
}
bool consumeFood(Player& player,Inventory& inventory,bool offhand,bool creative) {
    const auto held=offhand?inventory.equipment(4):inventory.selectedStack();
    if(held.empty() || !isFood(held.item) || player.hunger()>=20 || player.isSpectator() || player.isDead())return false;
    const auto& d=itemDefinition(held.item);
    if(!creative) {
        if(d.foodContainer!=Item::None) {if(!inventory.replaceHeld(offhand,held,{d.foodContainer,1}))return false;}
        else if(!(offhand?inventory.consumeOffhand():inventory.consumeSelected()))return false;
    }
    player.eat(d.foodValue,d.saturation);return true;
}
