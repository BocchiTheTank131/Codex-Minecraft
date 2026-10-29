#include "Adventure.h"
#include "UndergroundStructures.h"
#include "ExplorationLoot.h"
#include "TerrainDensity.h"
#include <algorithm>
#include <tuple>

RelicChamber relicChamber(const UndergroundPlan& p,std::uint32_t seed) {
    const auto& r=p.rooms.back();
    return {p.regionX,p.regionZ,{r.x,r.y,r.z},RelicPuzzle(TerrainDensity::hash(p.regionX,24,p.regionZ,seed^0xa2190U)%3)};
}
const char* relicPuzzleName(RelicPuzzle k) {
    return k==RelicPuzzle::Sequence?"THREE BELLS":k==RelicPuzzle::Routing?"DIVIDED CURRENT":"PULSE HALL";
}
void addRelicChamber(UndergroundPlan& p,std::uint32_t seed) {
    if(p.rooms.empty())return;
    const auto c=relicChamber(p,seed);const auto o=c.center;
    // The enlarged final room was included in reservation and clearance checks.
    // Everything remains inside its existing protected shell and foundation.
    std::map<std::tuple<int,int,int>,Block> cells;
    for(auto b:p.cells)cells[{b.x,b.y,b.z}]=b.block;
    auto put=[&](int x,int y,int z,Block b){cells[{o.x+x,o.y+y,o.z+z}]=b;};
    for(int z=-6;z<=6;++z)for(int x=-6;x<=6;++x)for(int y=1;y<5;++y)put(x,y,z,Block::Air);
    // A corner vault leaves every central corridor approach accessible.
    for(int z=2;z<=6;++z)for(int x=2;x<=6;++x)if(x==2||x==6||z==2||z==6)
        for(int y=1;y<5;++y)put(x,y,z,Block::StoneBricks);
    put(2,1,4,blockState(Block::OakFenceGate,1));put(2,2,4,Block::Air);
    put(5,1,4,Block::Chest);put(-5,1,4,Block::Bookshelf);
    put(-5,1,-5,Block::Torch);put(5,1,-5,Block::Torch);put(-5,1,5,Block::Torch);put(5,1,5,Block::Torch);
    if(c.kind!=RelicPuzzle::Sequence){
        put(0,1,-5,Block::StoneBricks);put(0,2,-5,Block::StoneBricks);put(0,3,-5,Block::PowerLamp);
        put(0,1,-6,Block::StoneBricks);put(0,2,-6,Block::StoneBricks);put(0,3,-6,Block::Lever);
        // Both pulse archetypes strike this strip. Keep the danger visible even
        // in the routing room; the rest of the floor remains a safe retreat.
        for(int x=-4;x<=4;++x)put(x,0,0,Block::TerracottaRed);
    }
    if(c.kind==RelicPuzzle::Sequence) {
        for(int n=0;n<3;++n){put(-3+n*3,1,-3,Block::Lever);
            // One/two/three gold inlays communicate the left-to-right sequence.
            for(int i=0;i<=n;++i)put(-3+n*3,0,-4-i,Block::TerracottaYellow);}
    } else if(c.kind==RelicPuzzle::Routing) {
        put(-3,1,-3,Block::Lever);put(3,1,-3,Block::Lever);
        for(int x:{-2,-1,1,2})put(x,1,-3,Block::PowerWire);
        put(0,1,-3,blockState(Block::AndGate,2));
        put(0,1,-2,Block::PowerWire);put(0,1,-1,Block::PowerLamp);
    } else {
        put(-3,1,-3,Block::OakPressurePlate);put(3,1,-3,Block::OakPressurePlate);
    }
    // Replace final-room loot/encounters only in V24. Historical plans never enter here.
    const auto& r=p.rooms.back();
    auto inRoom=[&](auto m){return std::abs(m.x-r.x)<=r.halfX&&std::abs(m.z-r.z)<=r.halfZ;};
    p.loot.erase(std::remove_if(p.loot.begin(),p.loot.end(),inRoom),p.loot.end());
    p.mobs.erase(std::remove_if(p.mobs.begin(),p.mobs.end(),inRoom),p.mobs.end());
    // Even a one-room dungeon needs an accessible key/cache outside its vault.
    // Never put the only seal behind the lock that requires it.
    put(-5,1,3,Block::Chest);
    p.loot.push_back({o.x-5,o.y+1,o.z+3,StructureLoot::RelicSupplies,TerrainDensity::hash(o.x,o.y,o.z,seed^0x2418U)});
    p.loot.push_back({o.x+5,o.y+1,o.z+4,StructureLoot::RelicVault,TerrainDensity::hash(o.x,o.y,o.z,seed^0x2419U)});
    p.cells.clear();p.cells.reserve(cells.size());
    for(auto [key,b]:cells){auto [x,y,z]=key;p.cells.push_back({x,y,z,b,b==Block::Air?TemplateOperation::Clear:TemplateOperation::Place});}
}

std::array<ItemStack,27> rollRelicVaultLoot(std::uint32_t seed,const StructureLootMarker& m) {
    // Independent V24-only stream: no historical roll or table is perturbed.
    std::array<ItemStack,27> out{};auto h=TerrainDensity::hash(m.x,m.y,m.z,seed^m.salt^0x219024U);
    out[0]={Item::AncientSeal,1};out[1]={Item::CavernShard,1};out[2]={Item::SurveyorToken,1};
    out[3]={Item::GoldIngot,2+int(h%3)};out[4]={Item::CookedBeef,3};out[5]={Item::Torch,12};
    if(m.table==StructureLoot::RelicSupplies)return out;
    out[8]={Item::ResonantCore,1};
    out[6]={h%5==0?Item::Diamond:Item::IronIngot,1+int(h%2)};
    if(h%8==0)out[7]={Item::EchoLantern,1,Inventory::maxDurability(Item::EchoLantern)/2};
    return out;
}
