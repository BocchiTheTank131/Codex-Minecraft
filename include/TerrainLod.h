#pragma once
#include "Block.h"
#include <algorithm>
#include <cmath>
namespace TerrainLod {
// Conservative far-detail LOD: rock/cave/structure/fluid surfaces never change.
// Level 1 omits distant grass/ferns; level 2 also omits tiny flowers/mushrooms.
// Leaves already have interior-face removal in the ordinary mesher.
// A horizon proxy is deliberately absent: it cannot preserve cave openings.
inline int desired(float distance,int renderDistance,int strength,int previous=0) {
    if(strength<=0 || distance<96.f)return 0;
    const float first=std::max(strength==1?384.f:strength==2?256.f:192.f,
        float(renderDistance*16)*(strength==1?.78f:strength==2?.60f:.48f));
    const float second=std::max(384.f,float(renderDistance*16)*(strength==1?1.5f:strength==2?.90f:.78f));
    const float firstH=first+(previous>=1?-32.f:32.f),secondH=second+(previous>=2?-32.f:32.f);
    return distance>=secondH?2:distance>=firstH?1:0;
}
inline bool smallDecoration(Block b) {
    return b==Block::TallGrass || b==Block::RedFlower || b==Block::YellowFlower ||
        b==Block::Fern || b==Block::DeadBush || b==Block::RedMushroom || b==Block::BrownMushroom;
}
inline bool omitDecoration(Block b,int level) {
    return level>=2?smallDecoration(b):level>=1 &&
        (b==Block::TallGrass || b==Block::Fern || b==Block::DeadBush);
}
}
