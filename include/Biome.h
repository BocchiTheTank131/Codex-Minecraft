#pragma once
#include "Block.h"
#include <cstdint>
#include <glm/glm.hpp>

enum class Biome : std::uint8_t {
    Plains, Forest, Desert, Mountains, SnowyPlains, Meadow, SnowySlopes,
    StonyPeaks, SnowyPeaks, BirchForest, Beach, Ocean,
    IceSpikes, SnowyTaiga, FrozenOcean, FrozenRiver, SnowyBeach, Taiga,
    OldGrowthTaiga, ColdOcean, StonyShore, DarkForest, FlowerForest,
    WindsweptHills, Jungle, SparseJungle, Swamp, Savanna, SavannaPlateau,
    DesertHills, Badlands, WoodedBadlands, MushroomFields, WarmOcean, River, Count
};
enum class ClimateRegion : std::uint8_t { Freezing, Cold, Temperate, Warm, Hot };
enum class TreeStyle : std::uint8_t { None, Oak, Birch, Spruce, GiantSpruce, Jungle, Acacia, DarkOak, Mushroom };
struct BiomeDefinition {
    const char* name;
    Block surface, soil;
    TreeStyle trees;
    int treeSpacing; // world-space candidate denominator; zero disables trees
    int decorationPercent;
    bool dry, frozen, naturalHostiles;
};
struct BiomeClimate {
    Biome biome = Biome::Plains;
    float temperature = .5f, humidity = .5f, effectiveTemperature = .5f;
    bool dry = false, freezes = false, naturalHostiles = true;
};
const BiomeDefinition& biomeDefinition(Biome biome);
ClimateRegion climateRegion(float temperature);
float altitudeTemperature(float temperature, float elevation);
Biome selectClimateBiome(float temperature, float humidity, float continentalness,
                         float erosion, float weirdness, float river, int height,
                         float mushroomIsland);
glm::vec3 climateGrassColor(const BiomeClimate& climate);
glm::vec3 climateFoliageColor(const BiomeClimate& climate);
glm::vec3 climateWaterColor(const BiomeClimate& climate);
