#pragma once
#include "Biome.h"
#include "Noise.h"

// V22-only geography. Climate sampling retains its historical world-space domain.
namespace GeographicDiversity {
struct Coast { float continent, detail; };
Coast coast(const PerlinNoise& noise, float warpedX, float warpedZ);
Biome habitat(Biome base, float temperature, float humidity, float regional, int height);
}
