#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Survival.h"

// Opt-in rendered fixture; exercises the real queued solver and hazard effects.
void Game::prepareHazardPreview() {
    world_->generate(2,{.5f,104.f,12.5f});
    for(int x:{-8,8})for(int z:{-2,7})world_->prepareSpawnTerrain({float(x),101.f,float(z)});
    for(int z=-2;z<=7;++z)for(int x=-8;x<=8;++x) {
        world_->setBlock(x,100,z,Block::Stone);
        for(int y=101;y<=104;++y) world_->setBlock(x,y,z,Block::Air);
    }
    for(int z:{0,4})for(int x=-8;x<=8;++x) {
        world_->setBlock(x,101,z-1,Block::Stone);world_->setBlock(x,101,z+1,Block::Stone);
    }
    world_->setBlock(-8,101,0,Block::Stone);world_->setBlock(-8,101,4,Block::Stone);
    world_->setBlock(-7,101,0,Block::Water);world_->setBlock(-7,101,4,Block::Lava);
    for(int n=0;n<140;++n)world_->updateFluids(.11f,4096);
    world_->setBlock(4,101,4,Block::Lava);
    // A glass test enclosure keeps the live mob in contact with the source.
    for(int y=101;y<=103;++y)for(const auto& d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)})
        world_->setBlock(4+d.x,y,4+d.y,Block::Glass);
    survival_->spawnStructureMob({4,101,4},true);
    inventory_->clear();inventory_->add(Item::CrackedStoneBricks,16);
    player_->teleport({.5f,108.f,12.5f});player_->setCreativeMode(true);player_->setFlying(true);
    timing_.worldTime=300.f;showDebug_=false;
}

#include <sstream>
bool World::runHabitatSmokeTest(std::string& report) {
    std::ostringstream result;bool passed=true;
    for(int scenario=0;scenario<4;++scenario) {
        World w(123456789,20);w.generate(2,{.5f,41.01f,.5f});w.setSimulationDistance(2);
        for(int z=-2;z<=1;++z)for(int x=-2;x<=1;++x) w.prepareSpawnTerrain({float(x*16)+.5f,41.f,float(z*16)+.5f});
        for(auto& entry:w.chunks_) {
            auto& c=*entry.second;c.blocks.assign(std::size_t((48-w.minimumY())*256),Block::Air);
            c.packedLight.assign(c.blocks.size(),0xf0);
            c.rebuildLightEmitterIndex();
            for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
                c.setLocal(x,40,z,Block::Stone);
                if(scenario>=2)c.setLocal(x,46,z,Block::Stone);
                if(scenario==3 && x%4==0 && z%4==0)c.setLocal(x,41,z,Block::Torch);
            }
            w.computeSunlight(c);
        }
        for(auto& entry:w.chunks_)w.rebuildBlockLightingNear(entry.second->x,entry.second->z);
        if(scenario==3)passed &= w.blockLightAt(1,42,1)>6;
        Player p({.5f,41.01f,.5f});p.setCreativeMode(true);Inventory inventory;SurvivalWorld sim(123456789);
        for(int batch=0;batch<12;++batch)sim.update(5.01f,w,p,inventory,scenario==1?0.f:1.f);
        const auto count=sim.diagnostics(p.position()).hostile;
        passed &= scenario==0||scenario==3?count==0:count>0;
        result<<(scenario==0?"day surface":scenario==1?"night surface":scenario==2?"day cave":"lit cave")<<'='<<count<<"; ";
    }
    report=result.str();return passed;
}
