#include "Game.h"
#include "Player.h"
#include "World.h"
#include "Survival.h"
#include "UiLayout.h"
#include "WorldTime.h"
#include <GLFW/glfw3.h>

// Explicit visual fixture only; normal startup never equips items automatically.
void Game::updateEquipmentPreview(double now) {
    const int stage=int((now-smokeTest_.startTime)/4.0);
    if(stage!=smokeTest_.stage) {
        smokeTest_.stage=stage;smokeTest_.worldgenScreenshotTaken=false;
        ui_.closeGameplayInterface(*inventory_);
        inventory_->clear();
        player_->setEquipment(inventory_.get());
        player_->setSpectatorMode(false);player_->setCreativeMode(false);
        creativeMode_=false;spectatorMode_=false;showDebug_=false;
        timing_.worldTime=WorldTime::DefaultSeconds;
        for(int z=-15;z<=12;++z)for(int x=-9;x<=9;++x) {
            world_->setBlock(x,239,z,Block::Stone);
            for(int y=240;y<=243;++y)world_->setBlock(x,y,z,Block::Air);
        }
        player_->teleport({.5f,240.f,7.5f});
        for(int part=0;part<4;++part) {
            inventory_->add(static_cast<Item>(int(Item::DiamondHelmet)+part),1,100+part*20);
            inventory_->equipFromSlot(0);
        }
        inventory_->add(Item::Shield,1,245);inventory_->equipFromSlot(0);
        inventory_->add(Item::IronSword,1);inventory_->add(Item::Torch,32);
        for(int material=0;material<4;++material) for(int part=0;part<4;++part)
            inventory_->add(static_cast<Item>(int(Item::LeatherHelmet)+material*4+part));
        if(stage==0) ui_.openInventory();
        if(stage==1) {creativeMode_=true;player_->setCreativeMode(true);ui_.openInventory();}
        if(stage==2) survival_->spawnBillboardPreview({.5f,240.f,7.5f},{0,0,-1});
        if(stage==3) {
            const glm::ivec3 chest{1,240,6};world_->setBlock(chest.x,chest.y,chest.z,Block::Chest);
            auto* data=world_->chestAt(chest,true);data->slots[0]={Item::IronChestplate,1,121};
            data->slots[1]={Item::Shield,1,156};ui_.openChest(chest);
        }
        synchronizeCursorCapture();
        if(stage<=1) {
            int W,H,ww,wh;glfwGetFramebufferSize(window_,&W,&H);glfwGetWindowSize(window_,&ww,&wh);
            const auto b=UiLayout::equipmentSlot(stage==1?UiMode::Creative:UiMode::Inventory,1,W,H);
            glfwSetCursorPos(window_,(b.x+20)*ww/W,(b.y+20)*wh/H);
        }
    }
    if(stage==2) {
        player_->setBlocking(true);
    }
    if(stage>=4) {glfwSetWindowShouldClose(window_,true);return;}
    if(now-smokeTest_.startTime-stage*4>1.5 && !smokeTest_.worldgenScreenshotTaken) {
        if(stage==2) player_->damagePhysical(4,PlayerDamageSource::Projectile,{.5f,240.9f,0});
        screenshotRequested_=true;smokeTest_.worldgenScreenshotTaken=true;
    }
}
