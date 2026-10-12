#include "BiomeBalance.h"
#include "TerrainAesthetics.h"

Biome BiomeBalance::variant(Biome base,float t,float h,float local,int y) {
    // Regional moisture selects a habitat family; the existing continuous
    // habitat field selects uncommon variants. No random per-column weights.
    switch(base) {
    case Biome::Forest:case Biome::BirchForest:case Biome::FlowerForest:case Biome::Plains:case Biome::Meadow:
        if(h>.60f && local>.26f)return Biome::FlowerForest;
        if(h>.40f && local>.15f)return Biome::BirchForest;
        if(local<-.19f)return y>82&&h>.40f?Biome::Meadow:Biome::Plains;
        return h>.38f&&local>-.14f?Biome::Forest:Biome::Plains;
    case Biome::Taiga:case Biome::OldGrowthTaiga:
        return h>.65f&&local>.24f?Biome::OldGrowthTaiga:Biome::Taiga;
    case Biome::Swamp:
        return h>.76f||y<48?Biome::Swamp:Biome::Forest;
    default:return TerrainAesthetics::variant(base,t,h,local,y);
    }
}
