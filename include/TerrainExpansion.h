#pragma once
#include "TerrainDensity.h"
#include "Biome.h"

// V23 only. Existing noise instances/lattice are inputs, never new mutable RNGs.
namespace TerrainExpansion {
float height(float continent, float erosion, float mountain, float broad,
             float rolling, float crag, float detail, float regional);
Biome habitat(Biome base, float temperature, float humidity, float regional,
              float local, int height, float erosion);
float density(const TerrainDensity::Fields& fields,
              const TerrainDensity::Column& column, int y, float slope);
float aquiferLevel(float aquifer);
float shaftAt(int x,int z,const PerlinNoise& first,const PerlinNoise& second);
}
