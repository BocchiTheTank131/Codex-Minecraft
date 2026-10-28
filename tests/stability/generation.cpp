#include "World.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <array>
#include <algorithm>
#include <sstream>
#include <queue>
#include <set>
#include <cmath>
int runExpansionTests(int argc,char** argv) {
    const auto output=argc>2?std::filesystem::absolute(argv[2]):std::filesystem::current_path();
    if(std::string(argv[1])=="--generation-routes" && argc>3) {
        std::filesystem::create_directories(output);std::ifstream source(argv[3]);std::string line;
        if(!source)return 2;std::getline(source,line);
        std::ofstream result(output/"route-accessibility.csv");
        result<<"seed,version,reachable_cells,reachable_biomes,nearest_other_biome_path,farthest_path\n";
        while(std::getline(source,line)) {
            std::istringstream row(line);std::vector<std::string> fields;std::string f;
            while(std::getline(row,f,','))fields.push_back(f);if(fields.size()<10)return 3;
            const auto seed=std::uint32_t(std::stoul(fields[0]));const auto version=unsigned(std::stoul(fields[1]));
            const int sx=int(std::floor(std::stof(fields[2]))),sz=int(std::floor(std::stof(fields[4])));
            World world(seed,version);constexpr int size=37,mid=size/2,spacing=64;
            struct Cell {int height;Biome biome;bool land;};std::array<Cell,size*size> cells{};
            const auto sample=[&](int x,int z) {
                const int h=world.terrainHeight(x,z);const auto b=world.biomeClimateAt(x,z).biome;
                const bool land=h>=67&&b!=Biome::River&&b!=Biome::FrozenRiver&&b!=Biome::Ocean&&
                    b!=Biome::ColdOcean&&b!=Biome::FrozenOcean&&b!=Biome::WarmOcean;
                return Cell{h,b,land};
            };
            for(int z=0;z<size;++z)for(int x=0;x<size;++x)cells[z*size+x]=sample(sx+(x-mid)*spacing,sz+(z-mid)*spacing);
            std::array<int,size*size> distances;distances.fill(-1);std::queue<int> todo;
            todo.push(mid*size+mid);distances[mid*size+mid]=0;std::set<Biome> biomes;int count=0,nearest=-1,farthest=0;
            const auto origin=cells[mid*size+mid].biome;
            while(!todo.empty()) {
                const int i=todo.front();todo.pop();const int x=i%size,z=i/size;++count;biomes.insert(cells[i].biome);
                farthest=std::max(farthest,distances[i]);if(nearest<0&&cells[i].biome!=origin)nearest=distances[i];
                for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                    const int nx=x+d.x,nz=z+d.y;if(nx<0||nx>=size||nz<0||nz>=size)continue;
                    const int next=nz*size+nx;if(distances[next]>=0||!cells[next].land)continue;
                    bool safe=true;int previous=cells[i].height;
                    for(int step=1;step<=4&&safe;++step) {
                        const auto c=sample(sx+(x-mid)*spacing+d.x*step*16,sz+(z-mid)*spacing+d.y*step*16);
                        safe=c.land&&std::abs(c.height-previous)<=8;previous=c.height;
                    }
                    if(safe){distances[next]=distances[i]+spacing;todo.push(next);}
                }
            }
            result<<seed<<','<<version<<','<<count<<','<<biomes.size()<<','<<nearest<<','<<farthest<<std::endl;
        }
        std::cout<<"Coarse connected spawn routes exported; not an exact collision/pathfinding proof"<<std::endl;return 0;
    }
    if(std::string(argv[1])=="--generation-overhangs") {
        std::filesystem::create_directories(output);std::ofstream sites(output/"overhangs.csv");
        sites<<"seed,x,y,z,camera_x,camera_y,camera_z\n";int found=0;
        for(const auto s:std::array<glm::ivec3,3>{{{20260917,1472,-440},{123456789,1992,-192},{42,1880,-1376}}}) {
            World world(unsigned(s.x),23);bool accepted=false;int probes=0;
            for(int dz=-240;dz<=240&&!accepted;++dz)for(int dx=-240;dx<=240&&!accepted;dx+=8) {
                if(dz%8)continue;
                const int x=s.y+dx,z=s.z+dz,h=world.terrainHeight(x,z);
                if(h<120)continue;
                for(const auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                    const int outside=world.terrainHeight(x+d.x*20,z+d.y*20);
                    if(h-outside<24)continue;
                    if(++probes>128)break;
                    world.prepareSpawnTerrain({float(x),float(h-30),float(z)});
                    for(int depth=12;depth<=56&&!accepted;depth+=2) {
                        const int y=h-depth;if(y<=outside+3)continue;
                        if(isSolid(world.getBlock(x,y,z))||!isSolid(world.getBlock(x,y+4,z)))continue;
                        bool clear=true;
                        for(int step=1;step<=20&&clear;++step) {
                            const glm::vec3 p(float(x+d.x*step),float(y),float(z+d.y*step));world.prepareSpawnTerrain(p);
                            clear=!isSolid(world.getBlock(int(p.x),y,int(p.z)))&&!isWater(world.getBlock(int(p.x),y,int(p.z)));
                        }
                        if(clear){sites<<s.x<<','<<x<<','<<y<<','<<z<<','<<x+d.x*20<<','<<y-1<<','<<z+d.y*20<<std::endl;accepted=true;++found;}
                    }
                    if(probes>128)break;
                }
                if(probes>128)break;
            }
            std::cout<<"Native exposed undercut seed "<<s.x<<" found="<<accepted<<std::endl;
        }
        return found?0:1;
    }
    World world(991,23);std::string report;
    const bool ok=world.runExpansionAudit(output.string(),report,std::string(argv[1]).substr(13));
    std::cout<<report<<std::endl;return ok?0:1;
}
