#include "TerrainExpansion.h"
#include <algorithm>
#include <cmath>

namespace TerrainExpansion {
namespace {
float smooth(float a,float b,float v) {
    const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);
}
}
float height(float c,float e,float mountain,float broad,float rolling,
             float crag,float detail,float regional) {
    const float inland=smooth(.015f,.16f,c), rugged=1-e;
    // Long ridge crests sit inside a broad, supported range mass. Regional
    // erosion changes both the summit profile and foothill width; passes follow
    // saddles in the same fields instead of independent circular peak masks.
    const float ridge=1-std::abs(broad);
    const float crest=smooth(.58f,.985f,ridge);
    const float range=mountain*(.45f+.55f*smooth(-.30f,.24f,regional));
    // A cusp at the ridge line retains a pointed silhouette. Squaring a
    // smoothstep flattened broad summits before the height shoulder even ran.
    const float pointed=std::clamp((ridge-.45f)/.55f,0.f,1.f);
    const float sharp=pointed*pointed*pointed;
    const float rounded=smooth(.38f,.92f,ridge);
    float h=66.f+c*82.f;
    h+=inland*rolling*(16.f+18.f*rugged);
    h+=range*(24.f+glm::mix(rounded,sharp,rugged)*(125.f+48.f*rugged));
    h+=inland*(.3f+.7f*rugged)*crag*(26.f+27.f*rugged);
    h+=range*crest*crag*30.f;
    // A minority of rugged range shoulders form escarpments; eroded regions
    // retain broad ramps and passes. The matching 3D notch supplies undercuts.
    h+=range*rugged*(smooth(-.045f,.025f,broad)-.5f)*38.f;
    // Broad elevated grasslands occupy eroded shoulders, with smooth ramps.
    const float plateau=inland*smooth(.42f,.72f,e)*smooth(.04f,.27f,regional);
    h=glm::mix(h,99.f+c*70.f+rolling*9.f+crag*5.f,plateau*.62f);
    h+=inland*detail*(1.f+4.f*rugged);
    return h;
}
Biome habitat(Biome b,float t,float h,float regional,float local,int y,float erosion) {
    const float effective=altitudeTemperature(t,float(y-16));
    // Exposed high ridges and cold environments remain geographical biomes.
    if(y>180 && erosion<.30f)
        return effective<.17f?Biome::SnowyPeaks:Biome::StonyPeaks;
    switch(b) {
    case Biome::Forest:case Biome::BirchForest:case Biome::FlowerForest:
    case Biome::Plains:case Biome::Meadow:case Biome::DarkForest:
        if(effective<.34f) return b;
        if(regional<-.18f)return y>100?Biome::Meadow:Biome::Plains;
        if(regional>.23f && h>.68f)return Biome::DarkForest;
        if(regional>.10f && h>.41f)return Biome::BirchForest;
        if(local>.22f && regional>-.02f && h>.5f)return Biome::FlowerForest;
        return h>.33f?Biome::Forest:b;
    case Biome::Jungle:case Biome::SparseJungle:
        return regional<-.16f?Biome::Forest:regional<.06f?Biome::SparseJungle:Biome::Jungle;
    case Biome::Taiga:case Biome::OldGrowthTaiga:
        return regional>.19f&&h>.52f?Biome::OldGrowthTaiga:Biome::Taiga;
    default:return b;
    }
}
float density(const TerrainDensity::Fields& f,const TerrainDensity::Column& c,int y,float slope) {
    const float depth=float(c.height-y)+.5f;
    float solid=depth/22.f;
    // A continuous roof and a lateral uphill attachment protect the surface.
    // Carving only subtracts rock: it cannot add disconnected floating lobes.
    const float roof=7.f+slope*4.f;
    const float cover=smooth(roof,roof+15.f,depth)*smooth(-60.f,-48.f,float(y));
    const float chamber=(f.chamber-.225f)*5.3f;
    const float tunnel=(.010f-f.tunnelA*f.tunnelA-f.tunnelB*f.tunnelB)*42.f;
    // Columns interrupt the largest cavities; narrow vertical veins join levels
    // only inside a chamber/tunnel envelope, rather than puncturing every roof.
    const float pillar=smooth(.16f,.34f,f.pillar);
    const float shaft=c.shaft*smooth(.08f,.22f,f.chamber);
    const float noodle=(.003f-f.surface*f.surface-f.vein*f.vein)*30.f*smooth(.23f,.38f,c.aquifer);
    const float cave=(std::max({chamber,tunnel,shaft,noodle})-pillar*.58f)*cover;
    if(cover>.01f && cave>0)solid=std::min(solid,-cave);
    const float cliff=smooth(1.f,2.8f,slope)*c.mountain;
    const float notch=smooth(roof,roof+5.f,depth)*(1-smooth(roof+14.f,roof+24.f,depth));
    solid-=cliff*notch*smooth(.03f,.30f,f.surface)*(1-smooth(.02f,.28f,f.pillar))*2.1f;
    const float bottom=float(c.height)-c.ravine*62.f;
    if(c.ravine>.02f&&y>bottom)solid=std::min(solid,(bottom-y)/12.f);
    return solid;
}
float aquiferLevel(float a) {
    // Rare, independently bounded underground lakes. Ordinary aquifers keep
    // their previous datum, far below riverbeds and surface entrance landings.
    // Entrances land at Y>=8. Keep all natural lake surfaces below those
    // checked dry approaches, including unusually strong aquifer samples.
    return std::min(6.f,-24.f+a*30.f+smooth(.36f,.55f,a)*20.f);
}
float shaftAt(int x,int z,const PerlinNoise& first,const PerlinNoise& second) {
    // Two horizontal fields intersect in compact patches. A single narrow
    // 3D scalar band cuts sheets, increasing geometry without making shafts.
    // Sampling once per column gives vertical coherence at no per-voxel noise
    // cost; chamber windows bound the depth and join existing cave levels.
    const float a=first.noise(x*.018f+173.7f,.417f,z*.018f-91.3f);
    const float b=second.noise(x*.018f-71.9f,.631f,z*.018f+137.1f);
    return (.004f-a*a-b*b)*60.f;
}
}
