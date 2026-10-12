#include "TerrainChaos.h"
#include <algorithm>
#include <cmath>

namespace TerrainChaos {
namespace {
float smooth(float a,float b,float v) {
    const float t=std::clamp((v-a)/(b-a),0.f,1.f);return t*t*(3-2*t);
}
}
float height(float c,float e,float mountain,float broad,float rolling,float crag,float detail) {
    const float inland=smooth(.015f,.16f,c),rugged=1-e;
    // Narrow signed-noise contour ridges rather than lifting an entire climate
    // region into a massif. Connected foothills remain below each local crest.
    const float ridge=smooth(.72f,.985f,1-std::abs(broad));
    const float crest=ridge*ridge;
    const float groups=.35f+.65f*smooth(-.32f,.22f,rolling);
    const float range=mountain*groups;
    const float relief=inland*(.22f+.78f*rugged);
    float h=66.f+c*82.f;
    h+=inland*rolling*(12.f+22.f*rugged);
    h+=range*(10.f+crest*(172.f+25.f*rugged));
    // Hills and valley shoulders vary on their own medium wavelength; they
    // remain gentle in highly eroded lowlands and stronger near rocky uplands.
    h+=relief*crag*(32.f+42.f*rugged);
    h+=range*crest*crag*28.f;
    h+=relief*detail*(2.f+7.f*rugged)*(.3f+.7f*ridge);
    return h;
}
float compress(float h) {
    // C1 continuous shoulder. Its asymptote leaves Y256 for snow; no flat
    // summit is produced by clipping a raw highland height to the ceiling.
    return h>200.f?200.f+55.f*(1-std::exp(-(h-200.f)/55.f)):h;
}
}
