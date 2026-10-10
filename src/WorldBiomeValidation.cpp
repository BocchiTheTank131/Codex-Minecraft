#include "World.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "Weather.h"
#include "Definitions.h"
#include "BiomeContent.h"

bool World::runBiomeAudit(const std::string& directory, std::string& report) const {
    for(int tile=-1;tile<BlockAtlasTiles;++tile) {
        VoxelMaterial material(static_cast<float>(tile));
        if(float(material)!=tile || (material.packed>>8U)!=0xffffffU) {
            report="Default packed terrain material";return false;
        }
        for(unsigned channel=0;channel<256;++channel) {
            const unsigned rgb=channel|((255-channel)<<8U)|((channel^85U)<<16U);
            material.setTint(rgb);
            if(float(material)!=tile || (material.packed>>8U)!=rgb) {
                report="Packed climate tint roundtrip";return false;
            }
        }
    }
    if(climateRegion(.17f)!=ClimateRegion::Cold || climateRegion(.34f)!=ClimateRegion::Temperate ||
       climateRegion(.60f)!=ClimateRegion::Warm || climateRegion(.80f)!=ClimateRegion::Hot ||
       biomeDefinition(Biome::Taiga).frozen || altitudeTemperature(.7f,220)>=.7f ||
       biomeDefinition(Biome::MushroomFields).naturalHostiles) {
        report="Climate thresholds or biome ecology";return false;
    }
    std::filesystem::create_directories(directory);
    std::ofstream out(directory + "/generation.csv");
    out << "seed,version,x,z,hash,ms\n";
    for (auto seed : {20260917U,123456789U,42U}) {
        World probe(seed);
        for (unsigned version = 1; version <= 9; ++version) {
            probe.generationVersion_ = version;
            for (int i=0;i<16;++i) {
                const int x=(i%4)*23-34, z=(i/4)*19-28;
                const auto start=std::chrono::steady_clock::now();
                const auto chunk=probe.generateChunkData(x,z);
                const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                std::uint64_t hash=14695981039346656037ULL;
                for (Block block : chunk.blocks) { hash ^= static_cast<unsigned char>(block); hash *= 1099511628211ULL; }
                out << seed << ',' << version << ',' << x << ',' << z << ',' << hash << ',' << ms << '\n';
            }
        }
    }

    std::array<unsigned,static_cast<int>(Biome::Count)> total{};
    std::array<std::uint64_t,static_cast<int>(Block::Count)> features{};
    std::ofstream locations(directory+"/biomes.csv");
    locations << "seed,biome,x,z,height,temperature,humidity\n";
    for(auto seed:{20260917U,123456789U,42U}) {
        World probe(seed);
        constexpr int side=320,step=96;
        std::array<unsigned,static_cast<int>(Biome::Count)> counts{};
        std::array<glm::ivec2,static_cast<int>(Biome::Count)> first{};
        const auto image=[&](const std::string& name) {
            std::ofstream file(directory+"/"+std::to_string(seed)+"-"+name+".ppm",std::ios::binary);
            file << "P6\n" << side << ' ' << side << "\n255\n";
            return file;
        };
        auto biomes=image("biomes"),temperature=image("temperature"),humidity=image("humidity"),terrain=image("terrain");
        for(int z=0;z<side;++z) for(int x=0;x<side;++x) {
            const int wx=(x-side/2)*step,wz=(z-side/2)*step;
            const auto sample=probe.sampleTerrain(wx,wz);
            const auto climate=probe.biomeClimateAt(wx,wz);
            const int b=static_cast<int>(sample.biome);
            if(!counts[b]++)first[b]={wx,wz}; ++total[b];
            // Prefer a forest/island interior for content checks and visual
            // previews rather than the first shallow shoreline of that biome.
            if(biomeDefinition(sample.biome).trees!=TreeStyle::None && sample.height>=SEA_LEVEL+5 &&
               probe.terrainHeight(first[b].x,first[b].y)<SEA_LEVEL+5) first[b]={wx,wz};
            if(sample.height<6||sample.height>=WORLD_HEIGHT-10 || climate.temperature<0||climate.temperature>1||
               climate.humidity<0||climate.humidity>1) {report="Climate or terrain bounds";return false;}
            const unsigned char rgb[]={static_cast<unsigned char>(45+(b*83)%180),static_cast<unsigned char>(45+(b*47)%180),static_cast<unsigned char>(45+(b*131)%180)};
            biomes.write(reinterpret_cast<const char*>(rgb),3);
            const auto pixel=[](std::ofstream& file,float value) {
                const unsigned char color[]={static_cast<unsigned char>(value*255),static_cast<unsigned char>((1-value)*200),100};
                file.write(reinterpret_cast<const char*>(color),3);
            };
            pixel(temperature,climate.temperature); pixel(humidity,climate.humidity);
            const unsigned char gray=static_cast<unsigned char>(sample.height);
            for(int c=0;c<3;++c) terrain.put(gray);
        }
        for(int b=0;b<static_cast<int>(Biome::Count);++b) if(counts[b]) {
            const auto position=first[b]; const auto c=probe.biomeClimateAt(position.x,position.y);
            locations << seed << ',' << biomeDefinition(static_cast<Biome>(b)).name << ',' << position.x << ',' << position.y << ','
                      << probe.terrainHeight(position.x,position.y) << ',' << c.temperature << ',' << c.humidity << '\n';
            // Cold/dry metadata and repeat generation are exercised at actual
            // seed-derived locations, rather than manufactured biome labels.
            Weather weather(seed);
            const auto precipitation=weather.precipitation(probe,position.x,position.y);
            if((c.dry && precipitation!=Precipitation::None)||(!c.dry && precipitation!=(c.freezes?Precipitation::Snow:Precipitation::Rain))) {
                report="Climate precipitation routing";return false;
            }
            const auto a=probe.generateChunkData(floorDiv(position.x,16),floorDiv(position.y,16));
            for(int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx) {
                const auto nearby=probe.generateChunkData(a.x+dx,a.z+dz);
                for(Block block:nearby.blocks) ++features[static_cast<int>(block)];
            }
            const auto again=probe.generateChunkData(a.x,a.z);
            if(a.blocks!=again.blocks) {report="Nondeterministic biome features";return false;}
        }
    }
    std::ofstream distribution(directory+"/distribution.csv");
    distribution << "biome,samples\n";
    int found=0;
    for(int b=0;b<static_cast<int>(Biome::Count);++b) {
        distribution << biomeDefinition(static_cast<Biome>(b)).name << ',' << total[b] << '\n';
        found+=total[b]>0;
    }

    if(found!=static_cast<int>(Biome::Count)) {report="Missing naturally selected biome";return false;}
    std::ofstream content(directory+"/features.csv"); content << "block,count\n";
    for(const auto& entry:biomeContent()) content << entry.name << ',' << features[static_cast<int>(entry.block)] << '\n';
    for(Block required:{Block::SpruceLog,Block::JungleLog,Block::AcaciaLog,Block::DarkOakLog,
        Block::Podzol,Block::Mycelium,Block::CoarseDirt,Block::PackedIce,Block::BlueIce,
        Block::TerracottaOrange,Block::Bamboo,Block::Fern,Block::DeadBush,Block::LilyPad,
        Block::RedMushroom,Block::BrownMushroom,Block::MushroomStem})
        if(features[static_cast<int>(required)]==0) {report="Missing natural content: "+itemDefinition(blockToItem(required)).displayName;return false;}
    // Appended item IDs and existing recipe/book paths share the same registry.
    Inventory inventory; inventory.clear();
    for(const auto& entry:biomeContent()) {
        Item decoded; Block decodedBlock; ItemSpriteUv uv;
        if(!itemFromSaveId(itemDefinition(entry.item).saveId,decoded)||decoded!=entry.item||
           !blockFromSaveId(blockDefinition(entry.block).saveId,decodedBlock)||decodedBlock!=entry.block||
           itemToBlock(entry.item)!=entry.block||!itemSpriteUv(entry.item,uv)) {
            report="Biome content registry or atlas mapping";return false;
        }
        inventory.add(entry.item,1);
    }
    const auto inventoryPath=directory+"/inventory.vxi";
    Inventory loaded;
    if(!inventory.save(inventoryPath,seed_)||!loaded.load(inventoryPath,seed_)) {report="New inventory persistence";return false;}
    for(const auto& entry:biomeContent()) if(loaded.count(entry.item)!=1) {report="New inventory ID lost";return false;}
    std::string crafting;
    if(!Inventory::runCraftingSelfTest(crafting)) {report=crafting;return false;}
    // Real world format roundtrip, with new IDs, followed by old-version loads.
    World saved(seed_); saved.generate(1);
    saved.synchronousMeshForSmokeTest_=false;
    for(int i=0;i<static_cast<int>(biomeContent().size());++i) {
        const int x=i%8,z=i/8;
        saved.setBlock(x,219,z,Block::Dirt);
        saved.setBlock(x,220,z,biomeContent()[i].block);
    }
    const auto worldPath=directory+"/world.vxw";
    if(!saved.saveWorld(worldPath,{.5f,230,.5f})) {report="World save";return false;}
    World restored(seed_); glm::vec3 player;
    if(!restored.loadWorld(worldPath,player)||restored.generationVersion()!=9) {report="Generator 9 load";return false;}
    restored.generate(1,player);
    for(int i=0;i<static_cast<int>(biomeContent().size());++i)
        if(restored.getBlock(i%8,220,i/8)!=biomeContent()[i].block) {report="New block persistence";return false;}
    for(unsigned version=1;version<=8;++version) {
        saved.generationVersion_=version;
        if(!saved.saveWorld(worldPath,player)||!restored.loadWorld(worldPath,player)||restored.generationVersion()!=static_cast<int>(version)) {
            report="Historical generator save compatibility";return false;
        }
    }
    std::cout << "New item/block save roundtrips and all recipes/autofill passed\n";
    std::cout << "Biome coverage " << found << "/" << static_cast<int>(Biome::Count) << '\n';
    report="Biome audit: compatibility hashes, climate maps, biome distribution and deterministic generation recorded";

    return bool(out);
}
