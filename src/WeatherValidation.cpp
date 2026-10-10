#include "Weather.h"
#include "WeatherRenderer.h"
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Renderer.h"
#include "Sound.h"
#include "Definitions.h"
#include "Persistence.h"
#include "Screenshot.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <psapi.h>
#endif

bool Weather::runSelfTest(World& world,SoundSystem& sounds,SurvivalWorld& survival,std::string& report) {
    const auto require=[](bool valid,const char* message){if(!valid) throw std::runtime_error(message);};
    Weather a(world.seed()),b(world.seed());
    require(a.remaining_>=240 && a.remaining_<=720,"clear duration");
    a.set(WeatherType::Rain); require(a.remaining_>=120 && a.remaining_<=300,"rain duration");
    a.set(WeatherType::Thunder); require(a.remaining_>=60 && a.remaining_<=180,"thunder duration");
    a.intensity_=.37f; a.storm_=.23f;
    a.fires_.push_back({{1,240,1},8.5f});
    std::stringstream save; a.write(save); require(b.read(save),"weather metadata read");
    std::stringstream roundtrip; b.write(roundtrip); require(roundtrip.str()==save.str(),"weather exact roundtrip");
    require(a.nextRandom()==b.nextRandom(),"weather random sequence persistence");
    std::stringstream oldSave; Weather old(world.seed()); require(!old.read(oldSave) && old.type()==WeatherType::Clear,"old weather metadata");
    std::stringstream corrupt("WEATHER1 2 nan 0 0 0 8 1"); require(!old.read(corrupt),"invalid weather metadata");
    const int x=0,z=0,y=240;
    require(world.hasLoadedChunkAt(x,z),"weather fixture chunk missing");
    const float prior=world.precipitationHeight(x,z);
    const Block original=world.getBlock(x,y,z);
    world.setBlock(x,y,z,Block::Stone);
    require(world.precipitationHeight(x,z)==241 && !exposed(world,{.5f,239,.5f}),"roof blocks rain");
    world.setBlock(x,y,z,Block::Glass); require(world.precipitationHeight(x,z)==241,"glass roof");
    world.setBlock(x,y,z,Block::WoodenSlab); require(world.precipitationHeight(x,z)==240.5f,"slab roof");
    world.setBlock(x,y,z,Block::LightningRod); require(world.precipitationHeight(x,z)==241,"rod exposure");
    world.setBlock(x,y,z,original); require(world.precipitationHeight(x,z)==prior,"roof removal metadata");
    world.prepareSpawnTerrain({.5f,240,.5f});
    std::vector<std::pair<glm::ivec3,Block>> spawnOriginal;
    for(int oz=-1;oz<=1;++oz) for(int ox=-1;ox<=1;++ox) for(int by=239;by<=244;++by) {
        spawnOriginal.push_back({{ox,by,oz},world.getBlock(ox,by,oz)});
        world.setBlock(ox,by,oz,by<=240 ? Block::Stone : by==241 ? snowLayerBlock(4) : Block::Air);
    }
    const auto snowySpawn=world.findSafeSpawnNear(0,0);
    require(glm::distance(snowySpawn,glm::vec3(.5f,241.51f,.5f))<.001f,"safe spawn on accumulated snow");
    for(const auto& saved : spawnOriginal) world.setBlock(saved.first.x,saved.first.y,saved.first.z,saved.second);
    Player listener({.5f,245,.5f}); listener.setCreativeMode(true);
    Weather delayed(world.seed());
    delayed.strike({30.5f,246,.5f},world,listener,survival,sounds);
    require(delayed.thunder_.size()==1 && std::abs(delayed.thunder_[0].delay-
        glm::distance(glm::vec3(30.5f,246,.5f),listener.cameraPosition())/343.0f)<.00001f,"distance-delayed thunder");
    GameSettings quiet; quiet.weatherCycle=false; quiet.snowAccumulation=false;
    delayed.update(.03f,world,listener,survival,sounds,quiet);
    require(delayed.thunder_.size()==1,"thunder must not play before sound arrives");
    delayed.update(.10f,world,listener,survival,sounds,quiet);
    require(delayed.thunder_.empty(),"delayed thunder dispatch");
    Weather fireTest(world.seed());
    std::vector<std::pair<glm::ivec3,Block>> fireOriginal;
    for(int oz=-1;oz<=1;++oz) for(int ox=-1;ox<=1;++ox) for(int by=240;by<=241;++by) {
        fireOriginal.push_back({{ox,by,oz},world.getBlock(ox,by,oz)});
        world.setBlock(ox,by,oz,by==240 ? Block::Planks : Block::Air);
    }
    for(int i=0;i<8;++i) fireTest.strike({.5f,241,.5f},world,listener,survival,sounds);
    require(!fireTest.fires_.empty() && fireTest.fires_.size()<=64,"bounded lightning ignition");
    for(const auto& saved : fireOriginal) world.setBlock(saved.first.x,saved.first.y,saved.first.z,saved.second);
    int rain=0,snow=0,dry=0; glm::ivec2 rainSite(0),snowSite(0),drySite(0);
    for(int cz=-4096;cz<=4096;cz+=128) for(int cx=-4096;cx<=4096;cx+=128) {
        // The ecology platforms are at Y=240, so choose climate for that
        // elevation instead of selecting a lowland rain site that becomes snow.
        const auto climate=world.biomeClimateAt(cx,cz,241);
        const auto kind=climate.dry?Precipitation::None:climate.freezes?Precipitation::Snow:Precipitation::Rain;
        if(kind==Precipitation::Rain && rain++==0) rainSite={cx,cz};
        if(kind==Precipitation::Snow && snow++==0) snowSite={cx,cz};
        if(kind==Precipitation::None && dry++==0) drySite={cx,cz};
    }
    require(rain>0,"rain biome distribution");
    std::cout<<"Biome samples: rain "<<rain<<", snow "<<snow<<", dry "<<dry<<'\n';
    GameSettings settings; a.intensity_=1; a.storm_=1;
    require(a.stormSpawning() && std::abs(a.daylightScale()-.2f)<.001f,"dark storm spawn/light state");
    const auto fixture=[&](glm::ivec2 site) {
        world.prepareSpawnTerrain({site.x+.5f,240,site.y+.5f});
        world.setBlock(site.x,240,site.y,Block::Stone);
        world.setBlock(site.x,241,site.y,Block::Air);
    };
    fixture(rainSite);
    world.setBlock(rainSite.x,240,rainSite.y,Block::Farmland);
    a.environmentColumn(world,rainSite.x,rainSite.y,settings);
    require(world.getBlock(rainSite.x,240,rainSite.y)==Block::WetFarmland,"rain hydrates farmland");
    world.setBlock(rainSite.x,242,rainSite.y,Block::Stone);
    world.setBlock(rainSite.x,240,rainSite.y,Block::Farmland);
    a.environmentColumn(world,rainSite.x,rainSite.y,settings);
    require(world.getBlock(rainSite.x,240,rainSite.y)==Block::Farmland,"sheltered farmland stays dry");
    world.setBlock(rainSite.x,242,rainSite.y,Block::Air);
    world.setBlock(rainSite.x,241,rainSite.y,Block::Fire);
    a.environmentColumn(world,rainSite.x,rainSite.y,settings);
    require(world.getBlock(rainSite.x,241,rainSite.y)==Block::Air,"rain extinguishes exposed fire");
    if(snow) {
        fixture(snowSite);
        for(int layer=1;layer<=8;++layer) {
            a.environmentColumn(world,snowSite.x,snowSite.y,settings);
            const Block block=world.getBlock(snowSite.x,241,snowSite.y);
            require(snowLayers(block)==layer && blockGeometry(block).maxY==layer*.125f,"snow depth/geometry");
        }
        world.setBlock(snowSite.x+1,241,snowSite.y,Block::Torch);
        for(int i=0;i<20 && world.blockLightAt(snowSite.x,241,snowSite.y)<12;++i)
            world.updateStreaming({snowSite.x+.5f,240,snowSite.y+.5f},0);
        a.environmentColumn(world,snowSite.x,snowSite.y,settings);
        require(snowLayers(world.getBlock(snowSite.x,241,snowSite.y))==7,"emitted light melts snow");
    }
    if(dry) {
        fixture(drySite);
        a.environmentColumn(world,drySite.x,drySite.y,settings);
        require(world.getBlock(drySite.x,241,drySite.y)==Block::Air,"dry biome has no snow");
    }
    if(world.generationVersion()>=9 && snow) {
        world.setBlock(snowSite.x+1,241,snowSite.y,Block::Air);
        for(int i=0;i<20 && world.blockLightAt(snowSite.x,241,snowSite.y)>=10;++i)
            world.updateStreaming({snowSite.x+.5f,240,snowSite.y+.5f},0);
        fixture(snowSite);
        world.setBlock(snowSite.x,240,snowSite.y,Block::Water);
        a.environmentColumn(world,snowSite.x,snowSite.y,settings);
        require(world.getBlock(snowSite.x,240,snowSite.y)==Block::Ice,"cold exposed water freezes");
        fixture(rainSite);
        world.setBlock(rainSite.x,240,rainSite.y,Block::Ice);
        a.environmentColumn(world,rainSite.x,rainSite.y,settings);
        require(world.getBlock(rainSite.x,240,rainSite.y)==Block::Water,"warm ordinary ice melts");
        for(Block permanent:{Block::PackedIce,Block::BlueIce}) {
            fixture(rainSite);
            world.setBlock(rainSite.x,240,rainSite.y,permanent);
            a.environmentColumn(world,rainSite.x,rainSite.y,settings);
            require(world.getBlock(rainSite.x,240,rainSite.y)==permanent,"building ice remains permanent");
        }
    }
    std::cout<<"Weather ecology: exposed/sheltered farmland, fire, snow depths/melting and dry biome passed\n";
    report="weather metadata/RNG roundtrip, legacy/corrupt saves, stone/glass/slab roofs, removal and rain/snow/dry biomes passed";
    return true;
}

void Game::runWeatherSmokeTest() {
    const auto require=[](bool valid,const char* message){if(!valid) throw std::runtime_error(message);};
    std::string report;
    Weather::runSelfTest(*world_,*sounds_,*survival_,report); std::cout<<report<<'\n';
    require(sounds_->verifyWeather(),"weather audio library");
    CommandContext context{*world_,*player_,*inventory_,*survival_,timing_.worldTime,seed_,
        [this]{return gameMode();},[this](GameMode m){setGameMode(m);},true,weather_.get(),sounds_.get()};
    for(const char* text : {"/weather clear 10","/weather rain 120","/weather thunder 100","/weather query"})
        require(commands_.execute(text,context).tone!=ChatTone::Error,"weather command");
    for(const char* text : {"/weather banana","/weather rain nan","/weather rain -1","/weather rain 999999999"})
        require(commands_.execute(text,context).tone==ChatTone::Error,"invalid weather command");
    require(!commands_.suggest("/weather t").empty(),"weather autocomplete");
    Inventory rods; rods.clear(); rods.add(Item::CopperIngot,3);
    bool crafted=false;
    for(std::size_t i=0;i<craftingRecipes().size();++i) if(craftingRecipes()[i].output==Item::LightningRod) {
        require(!rods.recipeCraftable(static_cast<int>(i),false),"rod requires table");
        require(rods.fillRecipe(static_cast<int>(i),true,false) && rods.craftingOutput(true).item==Item::LightningRod,"rod recipe/autofill");
        crafted=true;
    }
    require(crafted,"rod recipe missing");
    GameSettings settings=settings_; settings.weatherCycle=false; settings.snowAccumulation=false;
    const float remaining=weather_->remaining();
    for(int i=0;i<30;++i) weather_->update(.1f,*world_,*player_,*survival_,*sounds_,settings);
    require(weather_->remaining()==remaining && weather_->intensity()>.5f,"weather cycle toggle/transition");
    settings.weatherCycle=true;
    weather_->set(WeatherType::Clear,.1f);
    weather_->update(.2f,*world_,*player_,*survival_,*sounds_,settings);
    require(weather_->type()!=WeatherType::Clear,"automatic weather transition");
    Player target({.5f,245,.5f});
    world_->prepareSpawnTerrain(target.position());
    const glm::vec3 center=target.position()+glm::vec3(0,.9f,0);
    target.setCreativeMode(true);
    const float health=target.health(); weather_->strike(center,*world_,target,*survival_,*sounds_);
    require(target.health()==health,"creative lightning immunity");
    target.setSpectatorMode(true); weather_->strike(center,*world_,target,*survival_,*sounds_);
    require(target.health()==health,"spectator lightning immunity");
    target.setSpectatorMode(false); target.setCreativeMode(false);
    weather_->strike(center,*world_,target,*survival_,*sounds_);
    require(target.health()==health-5,"lightning survival damage");
    target.setCreativeMode(true);
    const Block rodOriginal=world_->getBlock(3,245,0);
    world_->setBlock(3,245,0,Block::LightningRod);
    weather_->strike({.5f,246,.5f},*world_,target,*survival_,*sounds_);
    require(weather_->bolts().back().position==glm::vec3(3.5f,246,.5f),"rod redirects strike");
    world_->setBlock(3,245,0,rodOriginal);
    require(commands_.execute("/summon lightning_bolt ~10 ~ ~10",context).tone!=ChatTone::Error,"summon lightning command");
    std::cout<<"Weather commands, cycle toggle/transition, lightning damage/immunity and rod crafting passed\n";
    player_->setCreativeMode(true);
    settings_.weatherCycle=false; settings_.snowAccumulation=false;
    weather_->set(WeatherType::Rain,300);
    for(int i=0;i<80;++i) weather_->update(.25f,*world_,*player_,*survival_,*sounds_,settings_);
    const double readyBy=glfwGetTime()+60;
    int ready=0;
    while(glfwGetTime()<readyBy && ready<30) {
        world_->updateStreaming(player_->cameraPosition(),8); renderFrame(.016f);
        ready=(world_->pendingChunkCount()==0 && world_->pendingCpuMeshCount()==0 && world_->queuedMeshRebuildCount()==0) ? ready+1 : 0;
    }
    require(ready==30,"weather visual scene did not settle");
    settings_.vsync=false; settings_.frameLimit=0; settings_.antiAliasingSamples=0; glfwSwapInterval(0);
    showDebug_=false;
    // Exercise all quality paths and settings geometry in the real GL context.
    for(int q=0;q<4;++q) {
        settings_.weatherQuality=q;
        for(int i=0;i<40;++i) renderFrame(1.0f/60);
        require(q!=0 || weatherRenderer_->particleCount()==0,"weather Off performs no particle work");
        require(glGetError()==GL_NO_ERROR,"weather rendering GL error");
    }
    ui_.openSettings(); ui_.openWeatherSettings();
    screenshotRequested_=true; renderFrame(0);
    ui_.resumeGame();
    renderer_->renderWeatherSettings(1280,720,42,settings_);
    require(glGetError()==GL_NO_ERROR,"weather settings render");
    if(weatherBenchmark_.empty()) {
        glm::ivec2 rainPosition(0);
        bool foundRain=false;
        for(int radius=0;radius<=512 && !foundRain;radius+=64)
            for(int z=-radius;z<=radius && !foundRain;z+=64)
                for(int x=-radius;x<=radius && !foundRain;x+=64)
                    if(Weather::precipitation(*world_,x,z)==Precipitation::Rain) {rainPosition={x,z};foundRain=true;}
        if(foundRain) {
            const auto position=world_->findSafeSpawnNear(rainPosition.x,rainPosition.y);
            world_->prepareSpawnTerrain(position); player_->teleport(position);
            const double readyRain=glfwGetTime()+60; int settled=0;
            while(glfwGetTime()<readyRain && settled<30) {
                world_->updateStreaming(player_->cameraPosition(),8); renderFrame(.016f);
                settled=(world_->pendingChunkCount()==0 && world_->pendingCpuMeshCount()==0 && world_->queuedMeshRebuildCount()==0) ? settled+1 : 0;
            }
            require(settled==30,"rain visual scene did not settle");
            settings_.weatherQuality=2;
            std::cout<<"Rain visual capture\n"; screenshotRequested_=true;renderFrame(0);
            weather_->set(WeatherType::Thunder,300);
            for(int i=0;i<80;++i) weather_->update(.25f,*world_,*player_,*survival_,*sounds_,settings_);
            std::cout<<"Thunder visual capture\n"; screenshotRequested_=true;renderFrame(0);
            weather_->strike(player_->position()+glm::vec3(6,0,0),*world_,*player_,*survival_,*sounds_);
            std::cout<<"Lightning visual capture\n"; screenshotRequested_=true;renderFrame(0);
        }
        std::cout<<"Weather Release smoke passed; graphics Off/Low/Medium/High, settings and embedded assets\n";
        return;
    }
    namespace fs=std::filesystem;
    fs::create_directories(weatherBenchmark_);
    std::ofstream out(fs::path(weatherBenchmark_)/"weather.csv");
    out<<"rd,quality,fps,mean_ms,p99_ms,worst_ms,simulation_ms,render_ms,particles,loaded,rendered,vertices,mesh_ms,upload_ms,ram_mb\n";
    std::vector<double> frames; frames.reserve(10000);
    player_->setCreativeMode(true); player_->setFlying(true);
    settings_.simulationDistance=8; world_->setSimulationDistance(8);
    for(int rd : {8,16,32,64}) {
        settings_.renderDistance=rd; world_->setRenderDistance(rd);
        const double deadline=glfwGetTime()+180;
        int stable=0;
        while(glfwGetTime()<deadline && stable<80) {
            world_->updateStreaming(player_->cameraPosition(),8);
            renderFrame(1.0f/60);
            if(world_->pendingChunkCount()==0 && world_->pendingCpuMeshCount()==0 &&
               world_->queuedMeshRebuildCount()==0) ++stable; else stable=0;
        }
        for(int q=0;q<4;++q) {
            settings_.weatherQuality=q;
            for(int i=0;i<60;++i) renderFrame(1.0f/60);
            frames.clear(); double simulation=0,render=0; std::size_t count=0;
            const double start=glfwGetTime();
            while(glfwGetTime()-start<4) {
                const double t=glfwGetTime();
                const float dt=beginFrame();
                sounds_->update(dt);
                timing_.worldTime=35;
                const double simStart=glfwGetTime();
                updateSimulation(dt);
                simulation+=(glfwGetTime()-simStart)*1000;
                const double renderStart=glfwGetTime();
                renderFrame(dt);
                render+=(glfwGetTime()-renderStart)*1000;
                frames.push_back((glfwGetTime()-t)*1000); ++count;
            }
            const double elapsed=glfwGetTime()-start;
            std::sort(frames.begin(),frames.end());
            // Sampling occurs outside timed frames and does not require F3.
#ifdef _WIN32
            PROCESS_MEMORY_COUNTERS counters{};
            if(GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))) timing_.workingSetBytes=counters.WorkingSetSize;
#endif
            out<<rd<<','<<q<<','<<count/elapsed<<','<<elapsed*1000/count<<','<<frames[static_cast<std::size_t>((count-1)*.99)]<<','<<frames.back()<<','<<simulation/count<<','<<render/count<<','
                <<weatherRenderer_->particleCount()<<','<<world_->loadedChunkCount()<<','<<world_->renderedChunkCount()<<','<<world_->uploadedVertexCount()<<','
                <<world_->lastChunkRebuildMilliseconds()<<','<<world_->lastMeshUploadMilliseconds()<<','<<timing_.workingSetBytes/(1024.0*1024)<<'\n';
            out.flush();
            if(q==0 || q==3) {screenshotRequested_=true;renderFrame(0);}

        }
    }
    weather_->set(WeatherType::Clear,300); settings_.weatherQuality=0;
    for(int i=0;i<200;++i) weather_->update(.25f,*world_,*player_,*survival_,*sounds_,settings_);
    const double control=glfwGetTime(); std::size_t controlFrames=0;
    while(glfwGetTime()-control<4) {
        const float dt=beginFrame(); sounds_->update(dt); timing_.worldTime=35;
        updateSimulation(dt); renderFrame(dt); ++controlFrames;
    }
    std::cout<<"RD64 Clear control: "<<(glfwGetTime()-control)*1000/controlFrames<<" ms\n";
    std::cout<<"Weather quality benchmarks saved to "<<weatherBenchmark_<<'\n';
}
