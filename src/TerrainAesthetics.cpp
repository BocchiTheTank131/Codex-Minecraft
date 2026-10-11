#include "TerrainAesthetics.h"
#include "TerrainExploration.h"
#include <algorithm>
#include <cmath>

namespace TerrainAesthetics {
namespace {
float smooth(float a,float b,float v) {const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);}
}
float density(const TerrainDensity::Fields& f,const TerrainDensity::Column& c,int y,float slope) {
    const float depth=float(c.height-y)+.5f;
    // No positive density above the connected surface envelope. A continuous
    // roof skin joins uphill into uncut terrain, rather than making a second
    // positive noise lobe above the surface as V11/V12 did.
    float solid=depth/22.f;
    // The roof must overlap the actual neighboring surface heights. This
    // local, world-coordinate constraint attaches steep caps laterally, even
    // when adjacent columns differ by more than the old three-block skin.
    const float roof=5.f+slope*4.f;
    const float cliff=smooth(.8f,2.5f,slope)*c.mountain;
    const float notch=smooth(roof,roof+4.f,depth)*(1-smooth(roof+13.f,roof+23.f,depth));
    const float recess=smooth(.04f,.29f,f.surface)*(1-smooth(.05f,.28f,f.pillar));
    solid-=cliff*notch*recess*1.55f;
    // Reuse the exact underground cave fields/curves. The near-surface roof
    // fades their influence, while deep cheese/tunnels/aquifers remain intact.
    const float cover=smooth(roof,roof+13.f,depth);
    const float chamber=(f.chamber-.29f)*5.f;
    const float tunnel=(.0075f-f.tunnelA*f.tunnelA-f.tunnelB*f.tunnelB)*42.f;
    const float pillar=smooth(.19f,.36f,f.pillar);
    const float noodle=(.003f-f.surface*f.surface-f.vein*f.vein)*30.f*smooth(.23f,.38f,c.aquifer);
    const float cave=(std::max({chamber,tunnel,noodle})-pillar*.45f)*cover;
    if(cover>.01f&&cave>0) solid=std::min(solid,-cave);
    const float bottom=float(c.height)-c.ravine*62.f;
    if(c.ravine>.02f&&y>bottom) solid=std::min(solid,(bottom-y)/12.f);
    return solid;
}
Biome variant(Biome base,float t,float h,float local,int y) {
    // Habitat varies within a climate family, never the macro temperature.
    switch(base) {
    case Biome::Forest: case Biome::BirchForest: case Biome::FlowerForest: case Biome::Plains: case Biome::Meadow:
        if(h>.34f&&local>.16f) return Biome::BirchForest;
        if(h>.34f&&local<-.19f) return y>72?Biome::Meadow:Biome::Plains;
        if(h>.51f&&local>.02f) return Biome::FlowerForest;
        if(h>.36f&&local>-.09f) return Biome::Forest;
        return base;
    case Biome::DarkForest:
        return local<-.15f?Biome::BirchForest:local>.20f?Biome::Forest:base;
    case Biome::Taiga: case Biome::OldGrowthTaiga:
        return h>.49f&&local>.11f?Biome::OldGrowthTaiga:Biome::Taiga;
    case Biome::SnowyTaiga: case Biome::SnowyPlains:
        return h>.30f&&local>-.04f?Biome::SnowyTaiga:Biome::SnowyPlains;
    case Biome::Jungle: case Biome::SparseJungle:
        return local<-.06f?Biome::SparseJungle:Biome::Jungle;
    case Biome::Savanna: case Biome::SavannaPlateau:
        return y>88&&local>.02f?Biome::SavannaPlateau:Biome::Savanna;
    default:return TerrainExploration::variant(base,t,h,local,y);
    }
}
}
