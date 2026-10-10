#include "BiomeVegetation.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int size=16, height=256, sea=48, reach=6;
std::uint32_t hash(int x,int z,std::uint32_t seed) {
    std::uint32_t h=seed ^ (std::uint32_t(x)*0x9e3779b9U) ^ (std::uint32_t(z)*0x85ebca6bU);
    h ^= h>>16; h*=0x7feb352dU; h ^= h>>15; h*=0x846ca68bU; return h^(h>>16);
}
struct Wood { Block log, leaves; };
Wood wood(TreeStyle style) {
    switch(style) {
    case TreeStyle::Birch: return {Block::BirchLog,Block::BirchLeaves};
    case TreeStyle::Spruce: case TreeStyle::GiantSpruce: return {Block::SpruceLog,Block::SpruceLeaves};
    case TreeStyle::Jungle: return {Block::JungleLog,Block::JungleLeaves};
    case TreeStyle::Acacia: return {Block::AcaciaLog,Block::AcaciaLeaves};
    case TreeStyle::DarkOak: return {Block::DarkOakLog,Block::DarkOakLeaves};
    default: return {Block::Log,Block::Leaves};
    }
}
}

void decorateBiomeChunk(int cx,int cz,std::uint32_t seed,std::vector<Block>& blocks,
                        const std::function<BiomeTerrain(int,int)>& terrain,
                        const std::function<bool(int,int)>& reserved,
                        const std::function<bool(int,int)>& supported) {
    const int ox=cx*size,oz=cz*size;
    auto at=[&](int x,int y,int z)->Block& { return blocks[(y*size+z)*size+x]; };
    auto place=[&](int x,int y,int z,Block value) {
        if(x<ox||x>=ox+size||z<oz||z>=oz+size||y<1||y>=height||reserved(x,z)) return;
        Block& current=at(x-ox,y,z-oz);
        if(current==Block::Air || current==Block::Snow || isPlant(current) ||
           (isWater(current) && (value==Block::PackedIce||value==Block::BlueIce))) current=value;
    };
    auto crown=[&](int x,int y,int z,int radius,Block leaf) {
        for(int dz=-radius;dz<=radius;++dz) for(int dx=-radius;dx<=radius;++dx)
            if(dx*dx+dz*dz<=radius*radius+1) place(x+dx,y,z+dz,leaf);
    };
    // All candidate roots within the maximum canopy reach are evaluated in
    // the same world order in neighboring chunks, without cross-chunk writes.
    for(int z=oz-reach;z<oz+size+reach;++z) for(int x=ox-reach;x<ox+size+reach;++x) {
        const auto h=hash(x,z,seed);
        // Cheap prefilter before sampling climate outside the column cache.
        if(h%37U && h%41U && h%43U && h%53U && h%67U && h%73U && h%83U &&
           h%97U && h%109U && h%127U && h%137U && h%157U && h%191U && h%211U &&
           h%223U && h%251U && h%307U && h%181U && h%233U) continue;
        const auto ground=terrain(x,z);
        const auto& biome=biomeDefinition(ground.biome);
        if(ground.height>=height-24) continue;
        const bool spike=ground.biome==Biome::IceSpikes && h%181U==0;
        const bool berg=ground.biome==Biome::FrozenOcean && h%233U==0;
        if(spike||berg) {
            if(reserved(x,z)) continue;
            const int base=berg?sea-3:ground.height;
            const int tall=(berg?5:9)+int((h>>8)%9U);
            for(int dy=1;dy<=tall;++dy) {
                const int radius=std::max(0,(tall-dy)/(berg?3:5));
                crown(x,base+dy,z,radius,dy<3?Block::BlueIce:Block::PackedIce);
            }
            continue;
        }
        if(!biome.treeSpacing || h%unsigned(biome.treeSpacing) || ground.height<=sea ||
           ground.river>.10f || biome.trees==TreeStyle::None) continue;
        if(reserved(x,z)) continue;
        const int slope=std::max({std::abs(terrain(x-1,z).height-ground.height),
            std::abs(terrain(x+1,z).height-ground.height),std::abs(terrain(x,z-1).height-ground.height),
            std::abs(terrain(x,z+1).height-ground.height)});
        if(slope>1) continue;
        if(!supported(x,z)) continue;
        const auto w=wood(biome.trees);
        const bool giant=biome.trees==TreeStyle::GiantSpruce||biome.trees==TreeStyle::Jungle;
        const bool thick=giant||biome.trees==TreeStyle::DarkOak;
        const int tall=giant?12+int((h>>8)%7U):biome.trees==TreeStyle::Spruce?7+int((h>>8)%3U):
                       biome.trees==TreeStyle::Mushroom?5+int((h>>8)%3U):5+int((h>>8)%3U);
        for(int dz=0;dz<(thick?2:1);++dz) for(int dx=0;dx<(thick?2:1);++dx) {
            const int bottom=thick?std::min(ground.height,terrain(x+dx,z+dz).height):ground.height;
            for(int y=bottom+1;y<=ground.height+tall;++y)
                place(x+dx,y,z+dz,biome.trees==TreeStyle::Mushroom?Block::MushroomStem:w.log);
        }
        const int top=ground.height+tall;
        if(biome.trees==TreeStyle::Spruce||biome.trees==TreeStyle::GiantSpruce) {
            for(int dy=-tall/2;dy<=1;++dy) crown(x,top+dy,z,dy>=0?1:std::min(4,1+(-dy)/2),w.leaves);
        } else if(biome.trees==TreeStyle::Acacia) {
            const int dx=(h&1)?1:-1,dz=(h&2)?1:-1;
            for(int b=1;b<=3;++b) place(x+dx*b,top-2+b/2,z+dz*b,w.log);
            crown(x+dx*3,top,z+dz*3,2,w.leaves); crown(x+dx*3,top+1,z+dz*3,1,w.leaves);
            crown(x-dx,top-1,z-dz,2,w.leaves);
        } else if(biome.trees==TreeStyle::Mushroom) {
            const Block cap=(h&1)?Block::RedMushroomBlock:Block::BrownMushroomBlock;
            crown(x,top+1,z,3,cap);
            if(cap==Block::RedMushroomBlock) for(int dy=-1;dy<=0;++dy)
                for(int dz=-3;dz<=3;++dz) for(int dx=-3;dx<=3;++dx)
                    if(std::max(std::abs(dx),std::abs(dz))==3) place(x+dx,top+dy,z+dz,cap);
        } else {
            const int radius=giant?4:biome.trees==TreeStyle::DarkOak?3:2;
            for(int dy=-2;dy<=1;++dy) crown(x,top+dy,z,dy==1?radius-1:radius,w.leaves);
            if(giant) {
                for(int b=1;b<=3;++b) place(x+b,top-4,z,w.log);
                crown(x+3,top-3,z,2,w.leaves);
                for(int dy=2;dy<tall;++dy) if((h+unsigned(dy))%3U)
                    place(x-1,ground.height+dy,z,Block::VineEast);
            }
        }
    }
    for(int z=0;z<size;++z) for(int x=0;x<size;++x) {
        const int wx=ox+x,wz=oz+z;
        const auto ground=terrain(wx,wz); const auto& biome=biomeDefinition(ground.biome);
        const auto h=hash(wx,wz,seed^0xdec0U);
        if(reserved(wx,wz)) continue;
        if(ground.biome==Biome::Swamp && ground.height<sea && h%17U==0 && at(x,sea,z)==Block::Water &&
           at(x,sea+1,z)==Block::Air) place(wx,sea+1,wz,Block::LilyPad);
        const int y=ground.height+1;
        if(y<=sea||y>=height-8||at(x,y,z)!=Block::Air||!isSolid(at(x,y-1,z))) continue;
        if(biome.dry) {
            if(h%73U==0) place(wx,y,wz,Block::DeadBush);
            else if((ground.biome==Biome::Desert||ground.biome==Biome::DesertHills)&&h%151U==0)
                for(int dy=0;dy<2+int((h>>8)%2);++dy) place(wx,y+dy,wz,Block::Cactus);
        } else if(h%100U<unsigned(biome.decorationPercent)) {
            Block plant=Block::TallGrass;
            if(biome.trees==TreeStyle::Spruce||biome.trees==TreeStyle::GiantSpruce) plant=Block::Fern;
            else if(ground.biome==Biome::MushroomFields||ground.biome==Biome::DarkForest||ground.biome==Biome::Swamp)
                plant=(h&1)?Block::RedMushroom:Block::BrownMushroom;
            else if(ground.biome==Biome::FlowerForest||ground.biome==Biome::Meadow || h%7U==0)
                plant=(h&1)?Block::RedFlower:Block::YellowFlower;
            if(ground.biome==Biome::Jungle && h%5U==0) {
                for(int dy=0;dy<3+int((h>>8)%4);++dy) place(wx,y+dy,wz,Block::Bamboo);
            } else place(wx,y,wz,plant);
        }
    }
}
