#include "RegionalClimate.h"
#include "Noise.h"
#include <algorithm>

float RegionalClimate::temperature(const PerlinNoise& noise, int x, int z,
                                  float macro, float thermal, float habitat) {
    // One additional column sample. The existing medium thermal sample bends
    // regional boundaries without additional warp noise or per-block work.
    const float regional = noise.noise(x * .00070f + 271.31f + thermal * .14f,
                                      .417f, z * .00070f - 163.79f - thermal * .19f);
    // Keep the macro pattern, but recover room for local variation where its
    // normalized field reaches 0/1. Fine habitat detail has very little weight.
    return std::clamp(.07f + macro * .86f + regional * .50f +
                      thermal * .14f + habitat * .018f, 0.f, 1.f);
}
