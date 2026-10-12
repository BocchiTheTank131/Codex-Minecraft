#include "World.h"
#include "TerrainDensity.h"
#include "TerrainAesthetics.h"
#include "BiomeVegetation.h"
#include "TerrainChaos.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace {
bool rock(Block b) {
    switch(b) {
    case Block::Stone:case Block::Dirt:case Block::Grass:case Block::Sand:case Block::Sandstone:
    case Block::SnowBlock:case Block::Gravel:case Block::Clay:case Block::Mud:case Block::Podzol:
    case Block::Mycelium:case Block::CoarseDirt:case Block::Granite:case Block::Diorite:case Block::Andesite:
    case Block::Deepslate:case Block::Bedrock:case Block::CoalOre:case Block::IronOre:case Block::CopperOre:
    case Block::GoldOre:case Block::DiamondOre:case Block::DeepslateCoalOre:case Block::DeepslateIronOre:
    case Block::DeepslateCopperOre:case Block::DeepslateGoldOre:case Block::DeepslateDiamondOre:
    case Block::Terracotta:case Block::TerracottaOrange:case Block::TerracottaBrown:case Block::TerracottaRed:
    case Block::TerracottaYellow:case Block::TerracottaWhite:return true;
    default:return false;
    }
}
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
}
bool World::runAestheticsAudit(const std::string& directory,std::string& report,unsigned modernVersion) const {
    try {
        std::filesystem::create_directories(directory);
        std::ofstream metrics(directory+"/connectivity.csv"),routes(directory+"/short-routes.csv"),locations(directory+"/captures.csv"),componentsFile(directory+"/components.csv");
        metrics<<"seed,version,scene,x,z,solid,exposed_faces,detached_components,detached_blocks,boundary_components,undercut_columns,ms,bytes,surface_detached_components,surface_detached_blocks,largest_detached\n";
        componentsFile<<"seed,version,scene,blocks,min_y,max_y,min_surface_depth,boundary\n";
        routes<<"seed,version,route,length,land_fraction,biome_types,transitions,min_height,max_height,height_std,mean_slope,steep_fraction,ridge_count\n";
        locations<<"seed,scene,x,z,height\n";
        std::ofstream flowers(directory+"/flowers.csv");
        std::ofstream climates(directory+"/climate-routes.csv");
        climates<<"seed,version,route,length,land_fraction,temperature_std,temperature_range,climate_transitions,max_adjacent_delta\n";
        flowers<<"seed,version,flowers,red,yellow,leaves,logs,columns\n";
        for(auto seed:{20260917U,123456789U,42U}) for(unsigned version:{modernVersion-1,modernVersion}) {
            std::size_t red=0,yellow=0,leaves=0,logs=0;
            for(int cz=-8;cz<8;++cz)for(int cx=-8;cx<8;++cx) {
                std::vector<Block> blocks(16*16*128,Block::Air);
                for(int y=0;y<=70;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x)
                    blocks[(y*16+z)*16+x]=y==70?Block::Grass:Block::Dirt;
                decorateBiomeChunk(cx,cz,seed,blocks,[](int,int){return BiomeTerrain{70,Biome::FlowerForest,0};},
                    [](int,int){return false;},[](int,int){return true;},0,128,64,version);
                for(auto b:blocks) {red+=b==Block::RedFlower;yellow+=b==Block::YellowFlower;leaves+=b==Block::Leaves;logs+=b==Block::Log;}
            }
            flowers<<seed<<','<<version<<','<<red+yellow<<','<<red<<','<<yellow<<','<<leaves<<','<<logs<<",65536\n";
        }
        if(modernVersion>=17) {
            std::ofstream vegetation(directory+"/vegetation-refinement.csv");
            vegetation<<"seed,version,biome,trees,bamboo,other_plants,columns\n";
            for(auto seed:{20260917U,123456789U,42U})for(int b=0;b<35;++b) {
                std::array<VegetationCounts,2> total{};
                std::array<std::size_t,2> other{};
                const int radius=Biome(b)==Biome::Jungle?16:4;
                for(int cz=-radius;cz<radius;++cz)for(int cx=-radius;cx<radius;++cx) {
                    std::array<std::vector<Block>,2> pair;
                    for(int v=0;v<2;++v) {
                        auto& blocks=pair[v];blocks.assign(16*16*128,Block::Air);
                        for(int y=0;y<=80;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x)
                            blocks[(y*16+z)*16+x]=y==80?Block::Grass:Block::Dirt;
                        decorateBiomeChunk(cx,cz,seed,blocks,[&](int,int){return BiomeTerrain{80,Biome(b),0};},
                            [](int,int){return false;},[](int,int){return true;},0,128,64,16+v,&total[v]);
                        for(auto block:blocks)other[v]+=isPlant(block)&&block!=Block::Bamboo;
                    }
                    for(int i=0;i<int(pair[0].size());++i)
                        require(!(isPlant(pair[1][i])&&pair[1][i]!=Block::Bamboo&&pair[1][i]!=pair[0][i]),
                            "Tree/bamboo thinning introduced replacement decoration");
                }
                for(int v=0;v<2;++v)vegetation<<seed<<','<<16+v<<','<<biomeDefinition(Biome(b)).name<<','
                    <<total[v].trees<<','<<total[v].bamboo<<','<<other[v]<<','<<radius*radius*1024<<'\n';
            }
        }
        for(auto seed:{20260917U,123456789U,42U}) {
            World modern(seed,modernVersion);
            // Find representative terrain, not a single hand-picked summit.
            std::array<glm::ivec2,10> scenes{};
            std::array<float,5> scores{};scores.fill(-1e9f);
            for(int z=-12000;z<=12000;z+=80)for(int x=-12000;x<=12000;x+=80) {
                const auto t=modern.sampleTerrain(x,z);
                if(t.river>.05f||t.ravine>.02f||t.height<76||t.height>268)continue;
                int type=-1;
                if(t.biome==Biome::Forest||t.biome==Biome::BirchForest||t.biome==Biome::Jungle)type=0;
                if(t.biome==Biome::Mountains||t.biome==Biome::StonyPeaks)type=1;
                if(t.biome==Biome::Savanna||t.biome==Biome::SavannaPlateau)type=2;
                if(t.biome==Biome::Taiga||t.biome==Biome::OldGrowthTaiga)type=3;
                const float relief=std::abs(float(modern.terrainHeight(x+32,z)-t.height));
                // Forest/savanna views should represent the habitat's interior,
                // not its last one-column strip immediately below a rock peak.
                const float score=(type==1?float(t.height):180.f-std::abs(float(t.height)-155.f))+
                    std::min(35.f,relief);
                if(type>=0&&score>scores[type]&&!modern.explorationStructureReservedAt(x,z)) {scores[type]=score;scenes[type]={x,z};}
            }
            scenes[4]={0,0};
            // Retain the fixed V12 regression sites after tuning. Re-selecting
            // only attractive V13 sites would conceal the old floating lobes.
            const std::array<glm::ivec2,5> fixed=seed==20260917U?
                std::array<glm::ivec2,5>{{{-9120,8800},{-10720,9040},{-7840,5680},{1920,8160},{0,0}}}:
                seed==123456789U?std::array<glm::ivec2,5>{{{-7600,3520},{-6720,4480},{-5040,-4560},{5680,720},{0,0}}}:
                std::array<glm::ivec2,5>{{{-5040,8880},{8560,-160},{-9200,-8160},{-10880,-1760},{0,0}}};
            std::copy(fixed.begin(),fixed.end(),scenes.begin()+5);
            for(int scene=0;scene<10;++scene) {
                require(scene>=4||scores[scene]>-1e8f,"Representative biome terrain missing");
                const auto center=scenes[scene];
                locations<<seed<<','<<scene<<','<<center.x<<','<<center.y<<','<<modern.terrainHeight(center.x,center.y)<<'\n';
                for(unsigned version:{modernVersion-1,modernVersion}) {
                    World probe(seed,version);
                    constexpr int n=96,h=384;
                    const int cx=floorDiv(center.x,16)-3,cz=floorDiv(center.y,16)-3;
                    std::vector<Block> volume(n*n*h,Block::Air);
                    const auto start=std::chrono::steady_clock::now();
                    std::size_t bytes=0;
                    for(int z=0;z<6;++z)for(int x=0;x<6;++x) {
                        const auto data=probe.generateChunkData(cx+x,cz+z);bytes+=data.blocks.size();
                        for(std::size_t i=0;i<data.blocks.size();++i) {
                            if(version>=15) require(i/256<321||data.blocks[i]==Block::Air,"Natural ceiling in terrain volume");
                            volume[(i/256*n+z*16+(i/16)%16)*n+x*16+i%16]=data.blocks[i];
                        }
                    }
                    const float ms=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-start).count();
                    const auto id=[](int x,int y,int z){return (y*n+z)*n+x;};
                    std::vector<std::uint8_t> occupied(volume.size());
                    std::size_t solid=0,faces=0;int undercuts=0;
                    for(std::size_t i=0;i<volume.size();++i) {occupied[i]=rock(volume[i]);solid+=occupied[i];}
                    for(int z=1;z<n-1;++z)for(int x=1;x<n-1;++x) {
                        bool cap=false,hole=false;
                        for(int y=h-2;y>=128;--y) {
                            const bool present=occupied[id(x,y,z)];
                            if(present&&!cap)cap=true;
                            if(!present&&cap)hole=true;
                            if(present&&hole) {++undercuts;break;}
                        }
                    }
                    for(int y=1;y<h-1;++y)for(int z=1;z<n-1;++z)for(int x=1;x<n-1;++x) if(occupied[id(x,y,z)])
                        for(int d:{-1,1,-n,n,-n*n,n*n}) faces+=!occupied[id(x,y,z)+d];
                    std::vector<int> flood;flood.reserve(occupied.size()/2);
                    std::size_t detached=0,components=0,boundary=0,surfaceDetached=0,surfaceComponents=0,largest=0;
                    std::array<int,n*n> surfaceHeights;
                    for(int z=0;z<n;++z)for(int x=0;x<n;++x)surfaceHeights[z*n+x]=probe.terrainHeight(cx*16+x,cz*16+z);
                    for(int i=0;i<int(occupied.size());++i) {
                        if(!occupied[i])continue;
                        flood.clear();flood.push_back(i);occupied[i]=0;
                        bool rooted=false,edge=false;int minY=320,maxY=-64,minDepth=384;
                        for(std::size_t j=0;j<flood.size();++j) {
                            const int v=flood[j],x=v%n,z=(v/n)%n,y=v/(n*n);
                            rooted|=y==0;edge|=x==0||x==n-1||z==0||z==n-1;
                            minY=std::min(minY,y-64);maxY=std::max(maxY,y-64);
                            minDepth=std::min(minDepth,surfaceHeights[z*n+x]-(y-64));
                            const auto visit=[&](int k){if(occupied[k]){occupied[k]=0;flood.push_back(k);}};
                            if(x>0)visit(v-1);if(x<n-1)visit(v+1);if(z>0)visit(v-n);if(z<n-1)visit(v+n);
                            if(y>0)visit(v-n*n);if(y<h-1)visit(v+n*n);
                        }
                        if(!rooted) {
                            componentsFile<<seed<<','<<version<<','<<scene<<','<<flood.size()<<','<<minY<<','<<maxY<<','<<minDepth<<','<<edge<<'\n';
                            if(!edge) {largest=std::max(largest,flood.size());
                                if(maxY>64&&minDepth<24){++surfaceComponents;surfaceDetached+=flood.size();}}
                            if(edge)++boundary;
                            else {++components;detached+=flood.size();}
                        }
                    }
                    metrics<<seed<<','<<version<<','<<scene<<','<<cx*16<<','<<cz*16<<','<<solid<<','<<faces<<','<<components<<','<<detached<<','<<boundary<<','<<undercuts<<','<<ms<<','<<bytes<<','<<surfaceComponents<<','<<surfaceDetached<<','<<largest<<'\n';
                    // Native blocks exported for shape/cutaway visualization;
                    // no post-generation terrain cleanup is done by the audit.
                    const auto name=directory+"/terrain-"+std::to_string(seed)+"-v"+std::to_string(version)+"-"+std::to_string(scene)+".bin";
                    std::ofstream file(name,std::ios::binary);file.write(reinterpret_cast<const char*>(volume.data()),volume.size());
                    require(bool(file),"Terrain volume export failed");
                }
            }
            for(unsigned version:{modernVersion-1,modernVersion}) {
                World probe(seed,version);
                for(int route=0;route<64;++route)for(int length:{500,1000,1500}) {
                    const int x=(route%8)*1600-5600,z=(route/8)*1600-5600;
                    std::set<Biome> biomes;int transitions=0,land=0,minY=320,maxY=-64,count=0;Biome previous=Biome::Count;
                    std::vector<int> heights;heights.reserve(190);double sum=0,sumSq=0,slope=0;int steep=0,ridges=0;
                    double thermalSum=0,thermalSquares=0,maxDelta=0;
                    float minT=1,maxT=0,lastT=0;int climateTransitions=0;
                    for(int d=0;d<=length;d+=8) {
                        const auto t=probe.sampleTerrain(x+d,z+d/3);biomes.insert(t.biome);
                        if(count) {
                            climateTransitions+=climateRegion(lastT)!=climateRegion(t.temperature);
                            maxDelta=std::max(maxDelta,double(std::abs(t.temperature-lastT)));
                        }
                        lastT=t.temperature;minT=std::min(minT,lastT);maxT=std::max(maxT,lastT);
                        thermalSum+=lastT;thermalSquares+=lastT*lastT;
                        transitions+=previous!=Biome::Count&&previous!=t.biome;previous=t.biome;
                        land+=t.height>=64;++count;minY=std::min(minY,t.height);maxY=std::max(maxY,t.height);
                        if(!heights.empty()) {const double rise=std::abs(t.height-heights.back())/8.;slope+=rise;steep+=rise>=1.;}
                        heights.push_back(t.height);sum+=t.height;sumSq+=t.height*t.height;
                    }
                    for(std::size_t i=3;i+3<heights.size();++i) {
                        const auto h=heights[i];bool peak=h>heights[i-1];
                        for(int j=-3;j<=3;++j)peak&=h>=heights[i+j];
                        ridges+=peak&&h-std::min(heights[i-3],heights[i+3])>=4;
                    }
                    const double deviation=std::sqrt(std::max(0.,sumSq/count-(sum/count)*(sum/count)));
                    routes<<seed<<','<<version<<','<<route<<','<<length<<','<<float(land)/count<<','<<biomes.size()<<','<<transitions<<','<<minY<<','<<maxY<<','
                        <<deviation<<','<<slope/(count-1)<<','<<double(steep)/(count-1)<<','<<ridges<<'\n';
                    climates<<seed<<','<<version<<','<<route<<','<<length<<','<<float(land)/count<<','
                        <<std::sqrt(std::max(0.,thermalSquares/count-std::pow(thermalSum/count,2)))<<','
                        <<maxT-minT<<','<<climateTransitions<<','<<maxDelta<<'\n';
                }
            }
            // Analytical surface constraint: no density island can be added
            // above the connected envelope, at any tested slope/noise value.
            for(int x=-128;x<=128;x+=8)for(int z=-128;z<=128;z+=8) {
                const auto t=modern.sampleTerrain(x,z);
                const TerrainDensity::Column c{t.height,1,0,-24,0};
                for(int y=t.height+1;y<std::min(320,t.height+54);y+=2)
                    require(TerrainAesthetics::density(TerrainDensity::sampleAt(x,y,z,modern.ridgeNoise_,modern.cheeseNoise_,modern.spaghettiNoise_,modern.noodleNoise_,modern.caveNoise_),c,y,4)<=0,"Density added unsupported above-surface rock");
            }
        }
        metrics.flush();routes.flush();locations.flush();
        require(bool(metrics)&&bool(routes)&&bool(locations),"Aesthetic measurements write failed");
        if(!runExplorationAudit(directory,report,modernVersion)) throw std::runtime_error(report);
        report+=" Versioned terrain connectivity, exposure, short routes and concurrent streaming measured.";
        return true;
    } catch(const std::exception& e) {report=e.what();return false;}
}
