#include "ExplorationLoot.h"
#include "TerrainDensity.h"
#include <algorithm>

namespace {
constexpr WeightedLootEntry common[] = {
    {Item::Bread,1,3,18,LootRarity::Common}, {Item::Apple,1,3,10,LootRarity::Common},
    {Item::Torch,3,9,22,LootRarity::Common}, {Item::Coal,2,6,16,LootRarity::Common},
    {Item::IronOre,1,4,12,LootRarity::Common}, {Item::CopperIngot,1,4,10,LootRarity::Common},
    {Item::SprucePlanks,3,10,8,LootRarity::Common}, {Item::StonePickaxe,1,1,4,LootRarity::Common}};
constexpr WeightedLootEntry uncommon[] = {
    {Item::IronIngot,1,4,35,LootRarity::Uncommon}, {Item::GoldIngot,1,3,15,LootRarity::Uncommon},
    {Item::IronPickaxe,1,1,12,LootRarity::Uncommon}, {Item::IronSword,1,1,12,LootRarity::Uncommon},
    {Item::CookedBeef,1,3,16,LootRarity::Uncommon}, {Item::Book,1,2,10,LootRarity::Uncommon}};
constexpr WeightedLootEntry rare[] = {
    {Item::Diamond,1,2,52,LootRarity::Rare}, {Item::SurveyorToken,1,1,20,LootRarity::Rare},
    {Item::CavernShard,1,1,16,LootRarity::Rare}, {Item::AncientSeal,1,1,12,LootRarity::Rare}};
}
std::array<ItemStack,27> rollExplorationLoot(std::uint32_t seed,const StructureLootMarker& marker) {
    std::array<ItemStack,27> out{};
    auto state=TerrainDensity::hash(marker.x,marker.y,marker.z,seed^marker.salt^0x10a712U);
    const auto next=[&]() {state=TerrainDensity::hash(int(state),0,int(state>>16U),0x519aU);return state;};
    const int deep=std::clamp((32-marker.y)/24,0,3);
    const bool treasure=marker.table==StructureLoot::Ancient;
    const int rolls=3+int(next()%3U)+(treasure?1:0);
    bool rareUsed=false;
    for(int roll=0;roll<rolls;++roll) {
        const int tier=int(next()%100U);
        const WeightedLootEntry* entries=common;int count=int(std::size(common));
        // At most one rare roll per chest. Depth increases chance, not abundance.
        if(!rareUsed&&tier<3+deep*2+(treasure?5:0)) {entries=rare;count=int(std::size(rare));rareUsed=true;}
        else if(tier<25+deep*6) {entries=uncommon;count=int(std::size(uncommon));}
        int weight=0;for(int i=0;i<count;++i)weight+=entries[i].weight;
        int choice=int(next()%unsigned(weight)),picked=0;
        for(;picked<count-1;++picked) {choice-=entries[picked].weight;if(choice<0)break;}
        const auto& e=entries[picked];
        int slot=int(next()%27U);while(!out[slot].empty())slot=(slot+1)%27;
        out[slot]={e.item,e.minimum+int(next()%unsigned(e.maximum-e.minimum+1)),Inventory::maxDurability(e.item)};
    }
    return out;
}
