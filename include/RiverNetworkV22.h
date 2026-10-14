#pragma once
#include "RiverNetwork.h"
namespace RiverNetworkV22 {
using RiverNetwork::Ground; using RiverNetwork::Sample; using RiverNetwork::Point; using RiverNetwork::Reach;
Sample sample(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground,float terrainHeight=64);
std::vector<Reach> inspectCell(int x,int z,std::uint32_t seed,const std::function<Ground(int,int)>& ground);
}
