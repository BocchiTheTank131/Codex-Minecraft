#include "GeographicDiversity.h"

namespace GeographicDiversity {
Coast coast(const PerlinNoise& noise, float x, float z) {
    // Two existing octave groups at shorter geographical wavelengths. The
    // subordinate coastal field bends straits/bays without per-column jitter.
    const float broad = noise.fractal3D(x*.00032f+.371f,.371f,z*.00032f-.613f,2,2.f,.23f);
    const float detail = noise.fractal2D(x*.00090f+91.417f,z*.00090f-37.283f,2,2.f,.25f);
    return {broad*.68f+detail*.32f-.025f, detail};
}
Biome habitat(Biome base,float t,float h,float regional,int height) {
    const float effective=altitudeTemperature(t,float(height-16));
    // Continuous habitat patches inside a climate family; special geographical
    // biomes, extreme climates and rare-biome eligibility are never overridden.
    switch(base) {
    case Biome::Jungle:case Biome::SparseJungle:
        if(regional<-.12f) return Biome::Forest;
        return regional<.09f?Biome::SparseJungle:Biome::Jungle;
    case Biome::DarkForest:
        return regional<-.10f?Biome::Forest:regional>.20f?Biome::BirchForest:base;
    case Biome::Forest:case Biome::Plains:case Biome::BirchForest:case Biome::Meadow:
        if(regional<-.15f) return height>100&&h>.40f?Biome::Meadow:Biome::Plains;
        if(h>.40f&&regional>.18f) return Biome::BirchForest;
        return h>.30f&&regional>-.06f?Biome::Forest:base;
    case Biome::Taiga:case Biome::OldGrowthTaiga:
        if(effective>=.27f&&regional<-.14f) return Biome::Forest;
        if(h>.65f&&regional>.24f) return Biome::OldGrowthTaiga;
        return regional<-.22f?Biome::WindsweptHills:Biome::Taiga;
    default:return base;
    }
}
}
