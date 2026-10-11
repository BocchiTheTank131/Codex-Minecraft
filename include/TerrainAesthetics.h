#pragma once
#include "TerrainDensity.h"
#include "Biome.h"

// V13 only. Surface mass is a continuous height field; 3D detail subtracts
// shallow, slope-masked undercuts instead of adding detached noise islands.
namespace TerrainAesthetics {
float density(const TerrainDensity::Fields& fields,const TerrainDensity::Column& column,int y,float slope);
Biome variant(Biome base,float temperature,float humidity,float habitat,int elevation);
}
