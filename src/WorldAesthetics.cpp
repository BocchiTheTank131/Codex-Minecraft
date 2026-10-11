#include "World.h"
#include "TerrainAesthetics.h"
#include <algorithm>
#include <cmath>

namespace {
float smooth(float a,float b,float v) {const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
}
World::TerrainSample World::sampleTerrainAesthetics(int x,int z) const {
    auto s=sampleTerrainGeography(x,z);
    const float local=biomeNoise_.fractal2D(x*.0045f+71.3f,z*.0045f-19.7f,2,2.f,.28f);
    const float wet=humidityNoise_.noise(x*.0029f-31.7f,0,z*.0029f+73.9f);
    s.humidity=std::clamp(s.macroHumidity+wet*.27f+local*.10f,0.f,1.f);
    s.temperature=std::clamp(s.macroTemperature+local*.014f,0.f,1.f);
    const float inland=smooth(.015f,.16f,s.continentalness);
    const float rugged=1-s.erosion;
    const float belt=1-smooth(.08f,.34f,std::abs(s.weirdness));
    const float mountain=smooth(.015f,.16f,s.naturalContinentalness)*belt*(1-smooth(.40f,.80f,s.erosion));
    const float broad=ridgeNoise_.fractal2D(x*.0016f-137.613f,z*.0016f+39.347f,2,2.f,.30f);
    const float rolling=terrainNoise_.fractal2D(x*.0030f+43.449f,z*.0030f-119.273f,2,2.f,.32f);
    const float ridge=1-std::abs(broad);
    const float crest=smooth(.48f,.97f,ridge);
    const float profile=crest*(.65f+.35f*crest);
    const float crag=terrainNoise_.noise(x*.0065f-61.37f,.319f,z*.0065f+117.91f);
    // Continuous range mass, with regional rounded/rugged profiles. Smaller
    // hills belong to foothills, not an independently floating 3D noise band.
    float height=66.f+s.continentalness*110.f;
    height+=inland*rolling*(9.f+20.f*rugged);
    height+=mountain*(18.f+profile*(100.f+38.f*rugged)+rugged*22.f);
    height+=mountain*rugged*(smooth(-.08f,.06f,broad)*20.f+crest*crag*26.f);
    height+=mountain*rugged*crest*(smooth(-.03f,.08f,crag)*34.f-17.f);
    height+=inland*belt*rugged*(8.f+rolling*12.f);
    // Erosion broadens the summits; dry uplands retain rolling plateaus.
    const float plateau=smooth(.60f,.82f,s.temperature)*(1-smooth(.28f,.48f,s.humidity))*inland;
    height=glm::mix(height,88.f+s.continentalness*45.f+rolling*12.f,plateau*.32f);
    // Keep the inherited coast/island/marsh and drainage datum. Valley banks
    // meet the river gradually, rather than cutting a vertical summit trench.
    if(s.continentalness<.025f||s.mushroomIsland>.1f||s.biome==Biome::Swamp)
        height=s.unclampedHeight+16.f;
    else height=std::max(height,66.f+smooth(.025f,.10f,s.continentalness)*3.f);
    const float valley=smooth(.0f,.55f,s.river);
    height=glm::mix(height,std::min(height,62.f),valley);
    if(height>264.f) height=264.f+38.f*(1-std::exp(-(height-264.f)/38.f));
    s.unclampedHeight=height;
    s.height=std::clamp(int(std::round(height)),-48,302);
    s.peakValley=profile*2-1;
    s.biome=selectGeographicalBiome(s.temperature,s.humidity,s.continentalness,s.erosion,local,s.river,s.height-16,s.mushroomIsland);
    // Elevation alone is not a bare-rock biome. Warm/wet foothills retain
    // their climate's vegetation up the mountain, with snowlines unchanged.
    const float effective=altitudeTemperature(s.temperature,float(s.height-16));
    if((s.biome==Biome::Mountains||s.biome==Biome::StonyPeaks) && s.height<278 && effective>=.17f) {
        if(s.temperature>.60f&&s.humidity<.38f) s.biome=Biome::SavannaPlateau;
        else if(s.humidity>.43f&&s.erosion>.14f) {
            if(effective<.34f) s.biome=Biome::Taiga;
            else if(s.temperature>.60f&&s.humidity>.75f) s.biome=Biome::Jungle;
            else if(s.temperature>.60f&&s.humidity>.61f) s.biome=Biome::SparseJungle;
            else s.biome=Biome::Forest;
        }
    }
    // A dry woodland belt buffers hot sand/terracotta against wet forest.
    // Coasts, water, snow and genuinely exposed peaks retain their identities.
    if(s.temperature>.80f&&s.humidity>.43f&&s.humidity<.60f&&s.height>72&&s.height<220&&
       s.river<.10f&&s.continentalness>.045f&&effective>=.34f&&
       s.biome!=Biome::StonyPeaks&&s.biome!=Biome::SnowyPeaks&&s.biome!=Biome::MushroomFields)
        s.biome=s.height>100?Biome::SavannaPlateau:Biome::Savanna;
    s.biome=TerrainAesthetics::variant(s.biome,s.temperature,s.humidity,local,s.height-16);
    const float contour=riverNoise_.noise(x*.0012f+63.7f,.713f,z*.0012f-131.9f);
    const float region=erosionNoise_.noise(x*.0007f-217.3f,0,z*.0007f+97.7f);
    s.ravine=(1-smooth(.003f,.016f,std::abs(contour)))*smooth(.05f,.19f,region)*
        smooth(.055f,.16f,s.continentalness)*(1-smooth(.1f,.45f,s.river));
    return s;
}

World::TerrainMemory World::terrainMemory() const {
    TerrainMemory result;
    for(const auto& page:terrainPages_) result.capacityBytes+=page->capacity*sizeof(VoxelVertex);
    for(const auto& entry:chunks_) {
        const auto& c=*entry.second;
        result.vertices+=c.opaqueVertexCount+c.waterVertexCount;
        result.capacityBytes+=c.waterBufferCapacity;
    }
    result.usedBytes=result.vertices*sizeof(VoxelVertex);
    return result;
}
