#include "Game.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "InventoryTooltip.h"
#include "Definitions.h"
#include "Persistence.h"
#include "Survival.h"
#include "UIManager.h"
#include "World.h"
#include "Player.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>

// Opt-in visual fixture, using the same inventory, mesh, atlas and recipe UI as gameplay.
void Game::updateCraftingPreview(double now) {
    const double elapsed = now - smokeTest_.startTime;
    const int stage = static_cast<int>(elapsed / 4.0);
    if (stage != smokeTest_.stage) {
        smokeTest_.stage = stage;
        smokeTest_.worldgenScreenshotTaken = false;
        if (stage == 0) {
            setGameMode(GameMode::Creative);
            timing_.worldTime = 105.0f;
            for (int z = -3; z <= 8; ++z)
                for (int x = -5; x <= 8; ++x) world_->setBlock(x,239,z,Block::Stone);
            for (int id = static_cast<int>(Block::CoalBlock); id < static_cast<int>(Block::Count); ++id) {
                const int index = id - static_cast<int>(Block::CoalBlock);
                world_->setBlock(-3+(index%5)*2,240,-2+(index/5)*2,static_cast<Block>(id));
            }
            player_->teleport({1.0f,242.0f,8.0f});
            player_->setFlying(true);
            player_->addMouseMovement(0,-150);
            showDebug_ = false;
        } else if (stage == 1) {
            inventory_->clear();
            for (Item item : {Item::Stone, Item::CoalOre, Item::Log, Item::IronBlock,
                              Item::Glass, Item::Bread, Item::Paper,
                              Item::DiamondPickaxe, Item::IronSword})
                inventory_->add(item, isTool(item) ? 1 : 16);
            ui_.openInventory();
        } else if (stage == 2) {
            ui_.closeGameplayInterface(*inventory_);
            setGameMode(GameMode::Survival);
            inventory_->clear();
            inventory_->add(Item::Planks,32);
            inventory_->add(Item::BirchPlanks,32);
            inventory_->add(Item::Book,12);
            for (int id=static_cast<int>(Item::Paper);id<static_cast<int>(Item::Count);++id)
                inventory_->add(static_cast<Item>(id),12);
            ui_.openInventory();
        } else if (stage == 3) {
            ui_.closeGameplayInterface(*inventory_);
            const glm::ivec3 table(1,240,6);
            world_->setBlock(table.x,table.y,table.z,Block::CraftingTable);
            player_->teleport({1.5f,240.0f,7.5f});
            ui_.openCraftingTable(table);
            const auto& recipes=craftingRecipes();
            for (int index=0;index<static_cast<int>(recipes.size());++index)
                if (recipes[index].output==Item::Bookshelf) inventory_->fillRecipe(index,true,false);
        } else if (stage == 4 || stage == 5) {
            ui_.closeGameplayInterface(*inventory_);
            const glm::ivec3 container(1,240,6);
            world_->setBlock(container.x,container.y,container.z,stage==4?Block::Chest:Block::Furnace);
            if(stage==4) {
                world_->chestAt(container,true)->slots[0]={Item::Diamond,5,0};
                ui_.openChest(container);
            } else {
                auto* furnace=world_->furnaceAt(container,true);
                furnace->input={Item::IronOre,10,0};furnace->fuel={Item::Coal,10,0};
                furnace->output={Item::IronIngot,3,0};
                ui_.openFurnace(container);
            }
        }
        if(stage>=1 && stage<=5) {
            synchronizeCursorCapture();
            int W=0,H=0;glfwGetFramebufferSize(window_,&W,&H);
            const auto b=stage==1 ? UiLayout::creativeItem(0,W,H) :
                stage==2 ? UiLayout::playerSlot(UiMode::Inventory,3,W,H) :
                stage==3 ? UiLayout::craftingOutput(W,H) :
                stage==4 ? UiLayout::chestSlot(0,W,H) :
                           UiLayout::furnaceSlot(UiSlotKind::FurnaceOutput,W,H);
            int ww=0,wh=0;glfwGetWindowSize(window_,&ww,&wh);
            glfwSetCursorPos(window_,(b.x+10)*ww/W,(b.y+10)*wh/H);
        }
    }
    if (!smokeTest_.worldgenScreenshotTaken && elapsed-stage*4.0>2.0) {
        screenshotRequested_=true;
        smokeTest_.worldgenScreenshotTaken=true;
    }
}

// Explicit developer smoke mode only; normal/demo startup never writes test data.
void Game::runCraftingContentSmokeTest() {
    std::string report;
    if (!Inventory::runCraftingSelfTest(report)) throw std::runtime_error(report);
    if (!UIManager::runRecipeBookSelfTest(report)) throw std::runtime_error(report);
    World testWorld(seed_);
    testWorld.generate(2, glm::vec3(.5f, 240, .5f));
    if (!testWorld.runCraftingContentSmokeTest(report)) throw std::runtime_error(report);
    std::cout << "Crafting expansion: " << craftingRecipes().size() << " recipes; " << report << '\n';
    Inventory inventory;
    inventory.clear();
    for (int id = static_cast<int>(Item::Paper); id < static_cast<int>(Item::Count); ++id) {
        const Item item = static_cast<Item>(id);
        inventory.add(item, 3);
        Item restored;
        if (!itemFromSaveId(itemDefinition(item).saveId, restored) || restored != item)
            throw std::runtime_error("new item stable ID");
        ItemSpriteUv uv;
        if (!itemSpriteUv(item, uv)) throw std::runtime_error("new item sprite UV");
    }
    for (int id = static_cast<int>(Block::CoalBlock); id < static_cast<int>(Block::Count); ++id) {
        const Block block = static_cast<Block>(id);
        testWorld.setBlock(id - static_cast<int>(Block::CoalBlock), 240, 0, block);
        Block restored;
        if (!blockFromSaveId(blockDefinition(block).saveId, restored) || restored != block)
            throw std::runtime_error("new block stable ID");
    }
    testWorld.setBlock(2, 238, 2, Block::Sand);
    testWorld.setBlock(3, 238, 2, Block::Water);
    if (!testWorld.canPlacePlant({2,239,2}, Block::SugarCane) ||
        testWorld.canPlacePlant({8,239,8}, Block::SugarCane))
        throw std::runtime_error("Sugar Cane support validation");
    testWorld.setBlock(4,239,4,Block::Stone);
    for (int facing = 0; facing < 4; ++facing) {
        constexpr glm::ivec3 positions[] = {{4,239,5},{4,239,3},{3,239,4},{5,239,4}};
        if (!testWorld.canPlacePlant(positions[facing], static_cast<Block>(static_cast<int>(Block::VineNorth)+facing)))
            throw std::runtime_error("Vine wall support validation");
    }
    const glm::ivec3 furnacePosition(6,240,4);
    testWorld.setBlock(6,240,4,Block::Furnace);
    FurnaceData* furnace = testWorld.furnaceAt(furnacePosition, true);
    for (Item fuel : {Item::Coal,Item::Charcoal,Item::CoalBlock}) {
        *furnace = FurnaceData{};
        furnace->input = {Item::ClayBall, 2, 0};
        furnace->fuel = {fuel, 1, 0};
        testWorld.updateBlockEntities(5.0f);
        if (furnace->output.item != Item::Brick || furnace->output.count != 1 ||
            furnace->fuelCapacity != furnaceFuelSeconds(fuel))
            throw std::runtime_error("furnace result/fuel regression");
    }
    for (int id = 1; id < static_cast<int>(Item::Count); ++id) {
        const Item item = static_cast<Item>(id);
        const Item result = smeltingResult(item);
        if (result == Item::None) continue;
        *furnace = FurnaceData{};
        furnace->input = {item, 1, 0};
        furnace->fuel = {Item::Charcoal, 1, 0};
        testWorld.updateBlockEntities(5.0f);
        if (furnace->output.item != result || furnace->output.count != 1 || !furnace->input.empty())
            throw std::runtime_error("furnace input consumption: " + itemDefinition(item).displayName);
    }
    if (Persistence::enabled()) {
        const char* inventoryPath = "crafting_content_test.vxi";
        const char* worldPath = "crafting_content_test.vxw";
        if (!inventory.save(inventoryPath, seed_) ||
            !testWorld.saveWorld(worldPath, glm::vec3(.5f,240,.5f)))
            throw std::runtime_error("new content save failed");
        Inventory restoredInventory;
        World restoredWorld(seed_);
        glm::vec3 position;
        if (!restoredInventory.load(inventoryPath, seed_) || !restoredWorld.loadWorld(worldPath, position))
            throw std::runtime_error("new content reload failed");
        restoredWorld.generate(2, position);
        for (int id = static_cast<int>(Item::Paper); id < static_cast<int>(Item::Count); ++id)
            if (restoredInventory.count(static_cast<Item>(id)) != 3)
                throw std::runtime_error("new inventory persistence");
        for (int id = static_cast<int>(Block::CoalBlock); id < static_cast<int>(Block::Count); ++id)
            if (restoredWorld.getBlock(id-static_cast<int>(Block::CoalBlock),240,0) != static_cast<Block>(id))
                throw std::runtime_error("new block persistence");
        std::remove(inventoryPath); std::remove(worldPath);
    }
    std::cout << "Crafting expansion: all 18 item IDs/icons, 15 block states, support, furnace fuels, "
              << (Persistence::enabled() ? "save/reload" : "demo persistence bypass") << " passed\n";
}

void Game::runPatch272SmokeTest() {
    std::string report;
    if(!world_->runPatch272SelfTest(report))throw std::runtime_error(report);
    std::cout << report << '\n';
    World origin(seed_);
    const glm::vec3 spawn=origin.findSafeSpawnNear(0,0);
    origin.generate(2,spawn);
    origin.prepareSpawnTerrain(spawn);
    // Every cause enters the common death/respawn path. Exercise the physical
    // origins that formerly leaked into respawn, including Creative flight.
    const glm::vec3 origins[]={{12000,240,9000},{0,240,0},{3000,8,-4000},
        {0,25,0},{-7000,200,7000},{1000,100,1000},{0,-25,0},{0,60,0}};
    for(int n=0;n<8;++n) {
        Player player(origins[n]);
        if(n==0){player.setCreativeMode(true);player.setFlying(true);player.setCreativeMode(false);}
        player.applyImpulse({5,-50,8});
        if(n==7)player.killByCommand();else player.damage(100.0f,
            n==5 ? PlayerDamageSource::Projectile : PlayerDamageSource::General);
        player.update(2.1f,PlayerInput{},origin);
        if(player.isDead() || glm::distance(player.position(),spawn)>.01f ||
           glm::length(player.velocity())>.01f || player.isFlying() || player.isSwimming() ||
           player.isSprinting() || player.isFalling())throw std::runtime_error("respawn state reset failed");
        player.damage(3,PlayerDamageSource::Projectile);
        if(player.health()!=20)throw std::runtime_error("respawn safety failed");
        for(int frame=0;frame<240;++frame)player.update(1.0f/60,PlayerInput{},origin);
        if(player.isDead() || player.health()!=20 || !player.isGrounded() || player.lastFallDamage()!=0)
            throw std::runtime_error("respawn death loop");
    }
    std::cout << "Respawn: eight death origins/causes, original world spawn, reset physics and safety passed\n";
    World distant(seed_);
    distant.generate(2,origins[0]);
    Player away(origins[0]);away.killByCommand();away.update(2.1f,PlayerInput{},distant);
    distant.prepareSpawnTerrain(away.position());
    for(int frame=0;frame<240;++frame)away.update(1.0f/60,PlayerInput{},distant);
    if(away.isDead() || !away.isGrounded() || away.health()!=20 || glm::distance(away.position(),spawn)>.05f)
        throw std::runtime_error("unloaded world-spawn respawn failed");
    std::cout << "Respawn: distant streamed world restores origin collision terrain before physics passed\n";
    Inventory inv;inv.clear();inv.add(Item::Sand,1);
    RecipeBookView book;book.entries={0};
    FurnaceData furnace;furnace.input={Item::IronOre,1,0};furnace.fuel={Item::Coal,1,0};furnace.output={Item::IronIngot,1,0};
    ChestData chest;chest.slots[0]={Item::Diamond,1,0};
    const auto check=[&](UiMode mode,UiRect bounds,Item expected,const RecipeBookView* b=nullptr,
                         const FurnaceData* f=nullptr,const ChestData* c=nullptr) {
        if(InventoryTooltip::hoveredItem(mode,1280,720,bounds.x+10,bounds.y+10,inv,b,f,c)!=expected)
            throw std::runtime_error("tooltip slot mapping failed");
    };
    check(UiMode::Inventory,UiLayout::playerSlot(UiMode::Inventory,0,1280,720),Item::Sand);
    check(UiMode::Inventory,UiLayout::playerSlot(UiMode::Inventory,1,1280,720),Item::None);
    check(UiMode::Creative,UiLayout::creativeItem(0,1280,720),creativeCatalog()[0]);
    check(UiMode::Inventory,UiLayout::recipeItem(0,1280,720),craftingRecipes()[0].output,&book);
    inv.add(Item::Planks,4);
    for(int i=0;i<static_cast<int>(craftingRecipes().size());++i)
        if(craftingRecipes()[i].output==Item::CraftingTable)inv.fillRecipe(i,false,false);
    check(UiMode::Inventory,UiLayout::craftingSlot(1,1280,720),Item::Planks);
    check(UiMode::Inventory,UiLayout::craftingOutput(1280,720),Item::CraftingTable);
    book.entries.assign(10,0);book.page=1;
    check(UiMode::CraftingTable,UiLayout::recipeItem(1,1280,720),craftingRecipes()[0].output,&book);
    check(UiMode::Chest,UiLayout::chestSlot(0,1280,720),Item::Diamond,nullptr,nullptr,&chest);
    for(auto kind:{UiSlotKind::FurnaceInput,UiSlotKind::FurnaceFuel,UiSlotKind::FurnaceOutput})
        check(UiMode::Furnace,UiLayout::furnaceSlot(kind,1280,720),
              kind==UiSlotKind::FurnaceInput?Item::IronOre:kind==UiSlotKind::FurnaceFuel?Item::Coal:Item::IronIngot,
              nullptr,&furnace);
    for(auto point:{glm::vec2(0),glm::vec2(1279,719),glm::vec2(640,360)}) {
        auto b=InventoryTooltip::bounds(1280,720,point.x,point.y,220);
        if(b.x<0||b.y<0||b.x+b.width>1280||b.y+b.height>720)throw std::runtime_error("tooltip edge bounds");
    }
    std::cout << "Tooltips: occupied/empty inventory, Creative, recipes, chest, furnace and screen edges passed\n";
}
