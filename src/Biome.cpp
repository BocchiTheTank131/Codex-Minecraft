#include "Biome.h"
#include <array>
#include <algorithm>

namespace {
using T=TreeStyle;
const std::array<BiomeDefinition,static_cast<int>(Biome::Count)> definitions{{
    {"PLAINS",Block::Grass,Block::Dirt,T::Oak,211,18,false,false,true},
    {"FOREST",Block::Grass,Block::Dirt,T::Oak,43,17,false,false,true},
    {"DESERT",Block::Sand,Block::Sandstone,T::None,0,5,true,false,true},
    {"MOUNTAINS",Block::Stone,Block::Stone,T::Spruce,191,5,false,false,true},
    {"SNOWY PLAINS",Block::SnowBlock,Block::Dirt,T::None,0,2,false,true,true},
    {"MEADOW",Block::Grass,Block::Dirt,T::Oak,307,38,false,false,true},
    {"SNOWY SLOPES",Block::SnowBlock,Block::Stone,T::Spruce,251,0,false,true,true},
    {"STONY PEAKS",Block::Stone,Block::Stone,T::None,0,0,false,false,true},
    {"SNOWY PEAKS",Block::SnowBlock,Block::Stone,T::None,0,0,false,true,true},
    {"BIRCH FOREST",Block::Grass,Block::Dirt,T::Birch,41,18,false,false,true},
    {"BEACH",Block::Sand,Block::Sand,T::None,0,0,false,false,true},
    {"OCEAN",Block::Sand,Block::Gravel,T::None,0,0,false,false,true},
    {"ICE SPIKES",Block::SnowBlock,Block::Dirt,T::None,0,0,false,true,true},
    {"SNOWY TAIGA",Block::SnowBlock,Block::Dirt,T::Spruce,37,4,false,true,true},
    {"FROZEN OCEAN",Block::Gravel,Block::Stone,T::None,0,0,false,true,true},
    {"FROZEN RIVER",Block::Gravel,Block::Clay,T::None,0,0,false,true,true},
    {"SNOWY BEACH",Block::SnowBlock,Block::Sand,T::None,0,0,false,true,true},
    {"TAIGA",Block::Grass,Block::Dirt,T::Spruce,37,27,false,false,true},
    {"OLD GROWTH TAIGA",Block::Podzol,Block::Dirt,T::GiantSpruce,83,32,false,false,true},
    {"COLD OCEAN",Block::Gravel,Block::Stone,T::None,0,0,false,false,true},
    {"STONY SHORE",Block::Stone,Block::Gravel,T::None,0,0,false,false,true},
    {"DARK FOREST",Block::Grass,Block::Dirt,T::DarkOak,53,24,false,false,true},
    {"FLOWER FOREST",Block::Grass,Block::Dirt,T::Oak,73,55,false,false,true},
    {"WINDSWEPT HILLS",Block::CoarseDirt,Block::Stone,T::Oak,223,7,false,false,true},
    {"JUNGLE",Block::Grass,Block::Dirt,T::Jungle,73,48,false,false,true},
    {"SPARSE JUNGLE",Block::Grass,Block::Dirt,T::Jungle,157,26,false,false,true},
    {"SWAMP",Block::Mud,Block::Clay,T::Oak,97,32,false,false,true},
    {"SAVANNA",Block::Grass,Block::CoarseDirt,T::Acacia,137,14,true,false,true},
    {"SAVANNA PLATEAU",Block::CoarseDirt,Block::Dirt,T::Acacia,109,8,true,false,true},
    {"DESERT HILLS",Block::Sand,Block::Sandstone,T::None,0,3,true,false,true},
    {"BADLANDS",Block::TerracottaOrange,Block::Terracotta,T::None,0,4,true,false,true},
    {"WOODED BADLANDS",Block::CoarseDirt,Block::Terracotta,T::Oak,127,5,true,false,true},
    {"MUSHROOM FIELDS",Block::Mycelium,Block::Dirt,T::Mushroom,67,30,false,false,false},
    {"WARM OCEAN",Block::Sand,Block::Sandstone,T::None,0,0,false,false,true},
    {"RIVER",Block::Clay,Block::Gravel,T::None,0,0,false,false,true}
}};
}
const BiomeDefinition& biomeDefinition(Biome biome) { return definitions.at(static_cast<int>(biome)); }
ClimateRegion climateRegion(float t) {
    return t<.17f?ClimateRegion::Freezing:t<.34f?ClimateRegion::Cold:
           t<.60f?ClimateRegion::Temperate:t<.80f?ClimateRegion::Warm:ClimateRegion::Hot;
}
float altitudeTemperature(float t,float height) {
    return std::clamp(t-std::max(0.f,height-86.f)*.0033f,0.f,1.f);
}
Biome selectClimateBiome(float t,float h,float c,float e,float w,float river,int y,float island) {
    const float effective=altitudeTemperature(t,float(y));
    if(island>.40f && y>=47) return Biome::MushroomFields;
    if(t>=.34f && t<.80f && h>.65f && y>=44 && y<61 && c>.025f && e>.40f) return Biome::Swamp;
    if(y<46) return t<.17f?Biome::FrozenOcean:t<.34f?Biome::ColdOcean:t>.72f?Biome::WarmOcean:Biome::Ocean;
    if(river>.62f && y<=53) return effective<.17f?Biome::FrozenRiver:Biome::River;
    if(y<=51 && c<.045f) return effective<.17f?Biome::SnowyBeach:
        (e<.30f && y>=49)?Biome::StonyShore:Biome::Beach;
    if(y>=140) return effective<.17f?Biome::SnowyPeaks:Biome::StonyPeaks;
    if(y>=100 && effective<.17f) return Biome::SnowySlopes;
    if(y>=108 && e<.53f) return Biome::Mountains;
    if(effective<.17f) return h>.50f?Biome::SnowyTaiga:w>.16f?Biome::IceSpikes:Biome::SnowyPlains;
    if(effective<.34f) return h>.65f?Biome::OldGrowthTaiga:h>.38f?Biome::Taiga:Biome::WindsweptHills;
    if(t>=.80f && h<.48f) {
        if(w>.10f && c>.05f) return h>.30f?Biome::WoodedBadlands:Biome::Badlands;
        return y>75?Biome::DesertHills:Biome::Desert;
    }
    if(t>.60f && h<.38f) return y>82?Biome::SavannaPlateau:Biome::Savanna;
    if(t>.60f && h>.75f) return Biome::Jungle;
    if(t>.60f && h>.61f) return Biome::SparseJungle;
    if(h>.65f && y<61 && e>.40f) return Biome::Swamp;
    if(y>=76 && h>.35f && h<.62f && e>.45f) return Biome::Meadow;
    if(y>84 && e<.32f) return Biome::WindsweptHills;
    if(h>.74f) return Biome::DarkForest;
    if(h>.59f) return w>.09f?Biome::FlowerForest:Biome::BirchForest;
    if(h>.43f) return Biome::Forest;
    return Biome::Plains;
}
glm::vec3 climateGrassColor(const BiomeClimate& c) {
    const glm::vec3 dry(.90f,.90f,.53f), lush(.48f,.93f,.54f), cold(.63f,.84f,.83f);
    return glm::mix(glm::mix(dry,lush,c.humidity),cold,std::clamp((.42f-c.effectiveTemperature)*1.7f,0.f,1.f));
}
glm::vec3 climateFoliageColor(const BiomeClimate& c) { return climateGrassColor(c)*glm::vec3(.93f,1.f,.94f); }
glm::vec3 climateWaterColor(const BiomeClimate& c) {
    if(c.biome==Biome::Swamp) return {.38f,.67f,.52f};
    return glm::mix(glm::vec3(.54f,.70f,1.f),glm::vec3(.44f,1.f,.85f),c.temperature);
}
