#include "Game.h"
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
