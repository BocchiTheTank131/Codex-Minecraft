#include "World.h"
#include <algorithm>
#include <cmath>

namespace {
float smooth(float a, float b, float value) {
    const float t = std::clamp((value - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Regional climate chooses the biome family; elevation and regional habitat
// choose compatible sub-biomes. This is deliberately separate from the v9 selector.
Biome geographicalBiome(float t, float h, float c, float e, float habitat,
                         float river, int y, float island) {
    const float effective = altitudeTemperature(t, static_cast<float>(y));
    if (island > .40f && c < -.10f && y >= SEA_LEVEL + 1) return Biome::MushroomFields;
    // Marshes include shallow water in warm/wet inland basins, so existing
    // aquatic vegetation (including lily pads) has real swamp habitat.
    if (t >= .34f && t < .80f && h > .65f && y >= SEA_LEVEL - 4 &&
        y < SEA_LEVEL + 13 && c > .025f && e > .40f) return Biome::Swamp;
    if (y < SEA_LEVEL - 2) return t < .17f ? Biome::FrozenOcean :
        t < .34f ? Biome::ColdOcean : t > .72f ? Biome::WarmOcean : Biome::Ocean;
    if (river > .62f && y <= SEA_LEVEL + 5) return effective < .17f ? Biome::FrozenRiver : Biome::River;
    if (y <= SEA_LEVEL + 3 && c < .045f) return effective < .17f ? Biome::SnowyBeach :
        (e < .30f && y >= SEA_LEVEL + 1) ? Biome::StonyShore : Biome::Beach;
    if (y >= 140) return effective < .17f ? Biome::SnowyPeaks : Biome::StonyPeaks;
    if (y >= 100 && effective < .17f) return Biome::SnowySlopes;
    if (y >= 108 && e < .53f) return Biome::Mountains;
    if (effective < .17f) return h > .50f ? Biome::SnowyTaiga :
        (habitat > .23f && t < .17f) ? Biome::IceSpikes : Biome::SnowyPlains;
    if (effective < .34f) return h > .65f ? Biome::OldGrowthTaiga :
        h > .38f ? Biome::Taiga : Biome::WindsweptHills;
    if (t >= .80f && h < .48f) {
        if (habitat > .10f && c > .05f) return h > .30f ? Biome::WoodedBadlands : Biome::Badlands;
        return y > 75 ? Biome::DesertHills : Biome::Desert;
    }
    if (t > .60f && h < .38f) return y > 82 ? Biome::SavannaPlateau : Biome::Savanna;
    if (t > .60f && h > .75f) return Biome::Jungle;
    if (t > .60f && h > .61f) return Biome::SparseJungle;
    if (h > .65f && y < 61 && e > .40f) return Biome::Swamp;
    if (y >= 76 && h > .35f && h < .62f && e > .45f) return Biome::Meadow;
    if (y > 84 && e < .32f) return Biome::WindsweptHills;
    if (h > .74f) return Biome::DarkForest;
    if (h > .59f) return habitat > .09f ? Biome::FlowerForest : Biome::BirchForest;
    if (h > .43f) return Biome::Forest;
    return Biome::Plains;
}
}

World::TerrainSample World::sampleTerrainGeography(int worldX, int worldZ) const {
    const float x = static_cast<float>(worldX), z = static_cast<float>(worldZ);
    TerrainSample sample;
    // Shared broad warp bends coasts, climate boundaries and mountain belts.
    // Its displacement is small relative to the continental wavelength.
    const float warpX = erosionNoise_.noise(x * .00014f + 17.31f, 0, z * .00014f - 29.73f) * 700.f;
    const float warpZ = erosionNoise_.noise(x * .00014f - 63.19f, 0, z * .00014f + 41.57f) * 700.f;
    const float wx = x + warpX, wz = z + warpZ;
    const float continent = continentalNoise_.fractal2D(wx * .00010f + .371f, wz * .00010f - .613f, 2, 2.f, .23f);
    const float coast = continentalNoise_.fractal2D(wx * .00045f + 91.417f, wz * .00045f - 37.283f, 2, 2.f, .25f);
    const float naturalContinent = continent * .88f + coast * .12f;
    const float starter = 1.f - smooth(160000.f, 1000000.f,
        (x * x + z * z) * std::clamp(1.f + coast * 2.f, .35f, 1.8f));
    // Guarantee a gentle starter shore if the origin lies in ocean. Do not
    // add a large artificial continental mountain on top of existing land.
    sample.continentalness = naturalContinent +
        std::max(0.f, .10f + coast * .10f - naturalContinent) * starter;
    sample.macroTemperature = std::clamp(.5f + temperatureNoise_.fractal2D(
        wx * .00012f + 129.337f, wz * .00012f - 83.719f, 2, 2.f, .20f) * 1.55f, 0.f, 1.f);
    sample.macroHumidity = std::clamp(.5f + humidityNoise_.fractal2D(
        wx * .00013f - 211.381f, wz * .00013f + 47.527f, 2, 2.f, .20f) * 1.65f, 0.f, 1.f);
    sample.erosion = smooth(-.38f, .38f, erosionNoise_.fractal2D(
        wx * .00022f - 53.271f, wz * .00022f + 117.439f, 2, 2.f, .25f));
    sample.weirdness = weirdnessNoise_.fractal2D(wx * .00020f + 67.673f, wz * .00020f + 173.291f, 2, 2.f, .25f);
    const float habitat = biomeNoise_.noise(wx * .00042f + 7.13f, 0, wz * .00042f - 73.19f);
    const float rolling = ridgeNoise_.fractal2D(x * .0009f + 43.449f, z * .0009f - 119.273f, 2, 2.f, .35f);
    // Microclimate can perturb a regional boundary, but cannot create a hot or
    // freezing region by itself. Altitude cooling is still applied centrally.
    sample.temperature = std::clamp(sample.macroTemperature + rolling * .018f, 0.f, 1.f);
    sample.humidity = std::clamp(sample.macroHumidity + coast * .025f, 0.f, 1.f);
    const float inland = smooth(.015f, .16f, sample.continentalness);
    const float rugged = 1.f - sample.erosion;
    const float belt = 1.f - smooth(.08f, .34f, std::abs(sample.weirdness));
    const float mountain = smooth(.015f, .16f, naturalContinent) * belt *
        (1.f - smooth(.40f, .80f, sample.erosion));
    const float ridge = 1.f - std::abs(ridgeNoise_.fractal2D(wx * .0013f - 137.613f, wz * .0013f + 39.347f, 2, 2.f, .35f));
    const float ridgeProfile = smooth(.58f, .97f, ridge);
    sample.peakValley = ridgeProfile * 2.f - 1.f;
    float height = SEA_LEVEL + 2.f + sample.continentalness * 110.f;
    height += inland * rolling * glm::mix(4.f, 11.f, rugged);
    height += mountain * (12.f + ridgeProfile * ridgeProfile * 100.f + rugged * 50.f);
    height += inland * belt * rugged * 10.f;
    height += inland * ridgeNoise_.noise(x * .0125f + 179.427f, 0, z * .0125f - 211.629f) * .8f;
    // Only deep ocean can form these rare secondary islands; ordinary coast
    // detail cannot lower inland terrain below sea level.
    sample.mushroomIsland = smooth(.27f, .41f, coast) * (1.f - smooth(-.22f, -.13f, sample.continentalness));
    height = glm::mix(height, SEA_LEVEL + 5.f + rolling * 3.f, sample.mushroomIsland);
    const float plateau = smooth(.72f, .88f, sample.temperature) *
        (1.f - smooth(.25f, .45f, sample.humidity)) * inland * smooth(.04f, .23f, habitat);
    height = glm::mix(height, 77.f + sample.continentalness * 30.f + rolling * 3.f, plateau * .65f);
    const float marsh = smooth(.65f, .82f, sample.humidity) * smooth(.40f, .62f, sample.erosion) *
        (1.f - smooth(55.f, 67.f, height)) * smooth(.0f, .06f, sample.continentalness) *
        smooth(.30f, .36f, sample.temperature) * (1.f - smooth(.76f, .82f, sample.temperature));
    height = glm::mix(height, SEA_LEVEL + 1.f + rolling * 2.f, marsh);
    // Single broad contour: local noise never splinters the drainage field.
    // The wide valley precedes the narrow water channel and highland headwaters
    // fade gradually instead of carving vertical trenches down from summits.
    // A rotated, non-lattice slice avoids axis-aligned zero contours from
    // the Perlin gradient lattice without adding fragmenting fine octaves.
    const float riverField = riverNoise_.noise((wx * .8660254f + wz * .5f) * .00038f + 241.359f,
        .371f, (-wx * .5f + wz * .8660254f) * .00038f - 197.473f);
    const float eligibility = smooth(-.12f, .025f, sample.continentalness) * (1.f - smooth(105.f, 180.f, height));
    const float core = 1.f - smooth(.003f, .014f, std::abs(riverField));
    const float bank = 1.f - smooth(.020f, .090f, std::abs(riverField));
    const float bed = static_cast<float>(SEA_LEVEL - 2);
    height -= std::max(0.f, height - bed) * bank * eligibility * .55f;
    sample.river = core * eligibility;
    height = glm::mix(height, std::min(height, bed), sample.river * sample.river);
    sample.height = std::clamp(static_cast<int>(std::round(height)), 6, WORLD_HEIGHT - 12);
    sample.biome = geographicalBiome(sample.temperature, sample.humidity, sample.continentalness,
        sample.erosion, habitat, sample.river, sample.height, sample.mushroomIsland);
    return sample;
}
