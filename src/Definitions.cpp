#include "Definitions.h"
#include "BiomeContent.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <initializer_list>

namespace {
std::string spacedName(const char* raw) {
    std::string name;
    for (const char* current = raw; *current; ++current) {
        if (current != raw && std::isupper(static_cast<unsigned char>(*current)))
            name.push_back(' ');
        name.push_back(*current);
    }
    return name;
}
using Items = std::array<ItemDefinition, static_cast<std::size_t>(Item::Count)>;
const Items& items() {
    static const Items definitions = [] {
        Items result{};
        auto put = [&](Item item, std::uint8_t id, int sprite, const char* name) {
            ItemDefinition& definition = result[static_cast<std::size_t>(item)];
            definition.displayName = spacedName(name);
            definition.saveId = id;
            definition.spriteIndex = sprite;
        };
        put(Item::None, 0, -1, "None");
        put(Item::Grass, 1, 0, "Grass");
        put(Item::Dirt, 2, 1, "Dirt");
        put(Item::Stone, 3, 2, "Stone");
        put(Item::Sand, 4, 3, "Sand");
        put(Item::Log, 5, 4, "Log");
        put(Item::Leaves, 6, 5, "Leaves");
        put(Item::Water, 7, 6, "Water");
        put(Item::CoalOre, 8, 7, "CoalOre");
        put(Item::IronOre, 9, 8, "IronOre");
        put(Item::GoldOre, 10, 9, "GoldOre");
        put(Item::CopperOre, 11, 10, "CopperOre");
        put(Item::DiamondOre, 12, 11, "DiamondOre");
        put(Item::Planks, 13, 12, "Planks");
        put(Item::Stick, 14, 13, "Stick");
        put(Item::CraftingTable, 15, 14, "CraftingTable");
        put(Item::Torch, 16, 15, "Torch");
        put(Item::WoodPickaxe, 17, 20, "WoodPickaxe");
        put(Item::StonePickaxe, 18, 21, "StonePickaxe");
        put(Item::IronPickaxe, 19, 22, "IronPickaxe");
        put(Item::WoodAxe, 20, 23, "WoodAxe");
        put(Item::StoneAxe, 21, 24, "StoneAxe");
        put(Item::IronAxe, 22, 25, "IronAxe");
        put(Item::WoodShovel, 23, 26, "WoodShovel");
        put(Item::StoneShovel, 24, 27, "StoneShovel");
        put(Item::IronShovel, 25, 28, "IronShovel");
        put(Item::Seeds, 26, 16, "Seeds");
        put(Item::Wheat, 27, 17, "Wheat");
        put(Item::Bread, 28, 18, "Bread");
        put(Item::RawMeat, 29, 19, "RawMeat");
        put(Item::RawBeef, 30, 30, "RawBeef");
        put(Item::RawPork, 31, 31, "RawPork");
        put(Item::Wool, 32, 32, "Wool");
        put(Item::CookedBeef, 33, 33, "CookedBeef");
        put(Item::CookedPork, 34, 34, "CookedPork");
        put(Item::Cobblestone, 35, 35, "Cobblestone");
        put(Item::StoneBricks, 36, 36, "StoneBricks");
        put(Item::Bricks, 37, 37, "Bricks");
        put(Item::Glass, 38, 38, "Glass");
        put(Item::Gravel, 39, 39, "Gravel");
        put(Item::Clay, 40, 40, "Clay");
        put(Item::Snow, 41, 41, "Snow");
        put(Item::SnowBlock, 42, 42, "SnowBlock");
        put(Item::BirchPlanks, 43, 43, "BirchPlanks");
        put(Item::BirchLog, 44, 44, "BirchLog");
        put(Item::BirchLeaves, 45, 45, "BirchLeaves");
        put(Item::Cactus, 46, 46, "Cactus");
        put(Item::Furnace, 47, 47, "Furnace");
        put(Item::Bookshelf, 48, 48, "Bookshelf");
        put(Item::WoodenDoor, 49, 49, "WoodenDoor");
        put(Item::WoodenSlab, 50, 50, "WoodenSlab");
        put(Item::StoneSlab, 51, 51, "StoneSlab");
        put(Item::Coal, 52, 52, "Coal");
        put(Item::IronIngot, 53, 53, "IronIngot");
        put(Item::GoldIngot, 54, 54, "GoldIngot");
        put(Item::Diamond, 55, 55, "Diamond");
        put(Item::CopperIngot, 56, 56, "CopperIngot");
        put(Item::Apple, 57, 57, "Apple");
        put(Item::Leather, 58, 58, "Leather");
        put(Item::RawMutton, 59, 59, "RawMutton");
        put(Item::CookedMutton, 60, 60, "CookedMutton");
        put(Item::Granite, 61, 61, "Granite");
        put(Item::Diorite, 62, 62, "Diorite");
        put(Item::Andesite, 63, 63, "Andesite");
        put(Item::MossyCobblestone, 64, 64, "MossyCobblestone");
        put(Item::MossyStoneBricks, 65, 65, "MossyStoneBricks");
        put(Item::Ice, 66, 66, "Ice");
        put(Item::Mud, 67, 67, "Mud");
        put(Item::TallGrass, 68, 68, "TallGrass");
        put(Item::RedFlower, 69, 69, "RedFlower");
        put(Item::YellowFlower, 70, 70, "YellowFlower");
        put(Item::Ladder, 71, 71, "Ladder");
        put(Item::Chest, 72, 72, "Chest");
        put(Item::GoldPickaxe, 73, 73, "GoldPickaxe");
        put(Item::GoldAxe, 74, 74, "GoldAxe");
        put(Item::GoldShovel, 75, 75, "GoldShovel");
        put(Item::DiamondPickaxe, 76, 76, "DiamondPickaxe");
        put(Item::DiamondAxe, 77, 77, "DiamondAxe");
        put(Item::DiamondShovel, 78, 78, "DiamondShovel");
        put(Item::WoodSword, 79, 79, "WoodSword");
        put(Item::StoneSword, 80, 80, "StoneSword");
        put(Item::IronSword, 81, 81, "IronSword");
        put(Item::GoldSword, 82, 82, "GoldSword");
        put(Item::DiamondSword, 83, 83, "DiamondSword");
        put(Item::Paper, 84, 84, "Paper");
        put(Item::Book, 85, 85, "Book");
        put(Item::ClayBall, 86, 86, "ClayBall");
        put(Item::Brick, 87, 87, "Brick");
        put(Item::Snowball, 88, 88, "Snowball");
        put(Item::Charcoal, 89, 89, "Charcoal");
        put(Item::SugarCane, 90, 90, "SugarCane");
        put(Item::Vine, 91, 91, "Vine");
        put(Item::CoalBlock, 92, 92, "CoalBlock");
        put(Item::IronBlock, 93, 93, "IronBlock");
        put(Item::GoldBlock, 94, 94, "GoldBlock");
        put(Item::CopperBlock, 95, 95, "CopperBlock");
        put(Item::DiamondBlock, 96, 96, "DiamondBlock");
        put(Item::HayBale, 97, 97, "HayBale");
        put(Item::Sandstone, 98, 98, "Sandstone");
        put(Item::PolishedGranite, 99, 99, "PolishedGranite");
        put(Item::PolishedDiorite, 100, 100, "PolishedDiorite");
        put(Item::PolishedAndesite, 101, 101, "PolishedAndesite");
        put(Item::LightningRod, 102, 102, "LightningRod");
        for(const auto& content : biomeContent()) {
            put(content.item, static_cast<std::uint8_t>(content.item), static_cast<int>(content.item), content.name);
            auto& definition=result[static_cast<std::size_t>(content.item)];
            definition.placedBlock=content.block;
            if(content.material==BiomeMaterial::Wood) definition.fuelSeconds=6;
        }
        for(Item log : {Item::SpruceLog,Item::JungleLog,Item::AcaciaLog,Item::DarkOakLog})
            result[static_cast<std::size_t>(log)].smeltedItem=Item::Charcoal;
        result[static_cast<std::size_t>(Item::Clay)].smeltedItem=Item::Terracotta;
        // Placement and furnace behavior share the authoritative item registry.
        const std::pair<Item, Block> placements[] = {
            {Item::Grass, Block::Grass}, {Item::Dirt, Block::Dirt},
            {Item::Stone, Block::Stone}, {Item::Sand, Block::Sand},
            {Item::Log, Block::Log}, {Item::Leaves, Block::Leaves},
            {Item::Water, Block::Water}, {Item::CoalOre, Block::CoalOre},
            {Item::IronOre, Block::IronOre}, {Item::GoldOre, Block::GoldOre},
            {Item::CopperOre, Block::CopperOre}, {Item::DiamondOre, Block::DiamondOre},
            {Item::Planks, Block::Planks}, {Item::CraftingTable, Block::CraftingTable},
            {Item::LightningRod, Block::LightningRod}, {Item::Torch, Block::Torch}, {Item::Cobblestone, Block::Cobblestone},
            {Item::StoneBricks, Block::StoneBricks}, {Item::Bricks, Block::Bricks},
            {Item::Glass, Block::Glass}, {Item::Gravel, Block::Gravel},
            {Item::Clay, Block::Clay}, {Item::Snow, Block::Snow},
            {Item::SnowBlock, Block::SnowBlock}, {Item::BirchPlanks, Block::BirchPlanks},
            {Item::BirchLog, Block::BirchLog}, {Item::BirchLeaves, Block::BirchLeaves},
            {Item::Cactus, Block::Cactus}, {Item::Furnace, Block::Furnace},
            {Item::Bookshelf, Block::Bookshelf}, {Item::WoodenDoor, Block::WoodenDoor},
            {Item::WoodenSlab, Block::WoodenSlab}, {Item::StoneSlab, Block::StoneSlab},
            {Item::Granite, Block::Granite}, {Item::Diorite, Block::Diorite},
            {Item::Andesite, Block::Andesite}, {Item::MossyCobblestone, Block::MossyCobblestone},
            {Item::MossyStoneBricks, Block::MossyStoneBricks}, {Item::Ice, Block::Ice},
            {Item::Mud, Block::Mud}, {Item::TallGrass, Block::TallGrass},
            {Item::RedFlower, Block::RedFlower}, {Item::YellowFlower, Block::YellowFlower},
            {Item::Ladder, Block::LadderNorth}, {Item::Chest, Block::Chest},
            {Item::SugarCane, Block::SugarCane}, {Item::Vine, Block::VineNorth},
            {Item::CoalBlock, Block::CoalBlock}, {Item::IronBlock, Block::IronBlock},
            {Item::GoldBlock, Block::GoldBlock}, {Item::CopperBlock, Block::CopperBlock},
            {Item::DiamondBlock, Block::DiamondBlock}, {Item::HayBale, Block::HayBale},
            {Item::Sandstone, Block::Sandstone}, {Item::PolishedGranite, Block::PolishedGranite},
            {Item::PolishedDiorite, Block::PolishedDiorite},
            {Item::PolishedAndesite, Block::PolishedAndesite}
        };
        for (const auto& entry : placements)
            result[static_cast<std::size_t>(entry.first)].placedBlock = entry.second;
        const std::pair<Item, Item> smelts[] = {
            {Item::IronOre, Item::IronIngot}, {Item::GoldOre, Item::GoldIngot},
            {Item::CopperOre, Item::CopperIngot}, {Item::CoalOre, Item::Coal},
            {Item::RawBeef, Item::CookedBeef}, {Item::RawPork, Item::CookedPork},
            {Item::RawMutton, Item::CookedMutton}, {Item::RawMeat, Item::CookedBeef},
            {Item::Sand, Item::Glass}, {Item::Cobblestone, Item::Stone},
            {Item::ClayBall, Item::Brick}, {Item::Log, Item::Charcoal},
            {Item::BirchLog, Item::Charcoal}
        };
        for (const auto& entry : smelts)
            result[static_cast<std::size_t>(entry.first)].smeltedItem = entry.second;
        result[static_cast<std::size_t>(Item::Coal)].fuelSeconds = 40.0f;
        result[static_cast<std::size_t>(Item::Charcoal)].fuelSeconds = 40.0f;
        result[static_cast<std::size_t>(Item::CoalBlock)].fuelSeconds = 400.0f;
        auto tool = [&](Item item, ToolKind kind, int tier, int durability, float damage,
                        float cooldown) {
            ItemDefinition& definition = result[static_cast<std::size_t>(item)];
            definition.maxStack = 1;
            definition.tool = kind;
            definition.tier = tier;
            definition.durability = durability;
            definition.attackDamage = damage;
            definition.attackCooldown = cooldown;
        };
        for (Item item : {Item::WoodPickaxe, Item::WoodAxe, Item::WoodShovel})
            tool(item, item == Item::WoodPickaxe ? ToolKind::Pickaxe :
                 item == Item::WoodAxe ? ToolKind::Axe : ToolKind::Shovel, 1, 60,
                 item == Item::WoodPickaxe ? 3.75f : item == Item::WoodAxe ? 6.0f : 3.0f,
                 item == Item::WoodAxe ? .85f : item == Item::WoodPickaxe ? .58f : .48f);
        for (Item item : {Item::StonePickaxe, Item::StoneAxe, Item::StoneShovel})
            tool(item, item == Item::StonePickaxe ? ToolKind::Pickaxe :
                 item == Item::StoneAxe ? ToolKind::Axe : ToolKind::Shovel, 2, 132,
                 item == Item::StonePickaxe ? 5.0f : item == Item::StoneAxe ? 8.0f : 4.0f,
                 item == Item::StoneAxe ? .85f : item == Item::StonePickaxe ? .58f : .48f);
        for (Item item : {Item::IronPickaxe, Item::IronAxe, Item::IronShovel})
            tool(item, item == Item::IronPickaxe ? ToolKind::Pickaxe :
                 item == Item::IronAxe ? ToolKind::Axe : ToolKind::Shovel, 3, 251,
                 item == Item::IronPickaxe ? 6.25f : item == Item::IronAxe ? 10.0f : 5.0f,
                 item == Item::IronAxe ? .85f : item == Item::IronPickaxe ? .58f : .48f);
        for (Item item : {Item::GoldPickaxe, Item::GoldAxe, Item::GoldShovel})
            tool(item, item == Item::GoldPickaxe ? ToolKind::Pickaxe :
                 item == Item::GoldAxe ? ToolKind::Axe : ToolKind::Shovel, 1, 33,
                 item == Item::GoldPickaxe ? 3.75f : item == Item::GoldAxe ? 6.0f : 3.0f,
                 item == Item::GoldAxe ? .85f : item == Item::GoldPickaxe ? .58f : .48f);
        for (Item item : {Item::DiamondPickaxe, Item::DiamondAxe, Item::DiamondShovel})
            tool(item, item == Item::DiamondPickaxe ? ToolKind::Pickaxe :
                 item == Item::DiamondAxe ? ToolKind::Axe : ToolKind::Shovel, 4, 1561,
                 item == Item::DiamondPickaxe ? 7.5f : item == Item::DiamondAxe ? 12.0f : 6.0f,
                 item == Item::DiamondAxe ? .85f : item == Item::DiamondPickaxe ? .58f : .48f);
        tool(Item::WoodSword, ToolKind::Sword, 1, 60, 5.0f, .55f);
        tool(Item::StoneSword, ToolKind::Sword, 2, 132, 6.0f, .55f);
        tool(Item::IronSword, ToolKind::Sword, 3, 251, 7.0f, .55f);
        tool(Item::GoldSword, ToolKind::Sword, 1, 33, 5.0f, .55f);
        tool(Item::DiamondSword, ToolKind::Sword, 4, 1561, 8.0f, .55f);
        result[static_cast<std::size_t>(Item::None)].maxStack = 0;
        for (Item item : {Item::RawMeat, Item::RawBeef, Item::RawPork, Item::RawMutton})
            result[static_cast<std::size_t>(item)].foodValue = 3.0f;
        for (Item item : {Item::CookedBeef, Item::CookedPork, Item::CookedMutton})
            result[static_cast<std::size_t>(item)].foodValue = 7.0f;
        result[static_cast<std::size_t>(Item::Bread)].foodValue = 5.0f;
        result[static_cast<std::size_t>(Item::Apple)].foodValue = 4.0f;
        return result;
    }();
    return definitions;
}
using Blocks = std::array<BlockDefinition, static_cast<std::size_t>(Block::Count)>;
const Blocks& blocks() {
    static const Blocks definitions = [] {
        Blocks result{};
        auto put = [&](Block block, std::uint8_t id, int top, int side, int bottom, Item drop) {
            BlockDefinition& definition = result[static_cast<std::size_t>(block)];
            definition.saveId = id;
            definition.topTexture = top;
            definition.sideTexture = side;
            definition.bottomTexture = bottom;
            definition.drop = drop;
            definition.geometry = blockGeometry(block);
            definition.solid = isSolid(block);
            definition.transparent = isTransparentBlock(block);
        };
        for(const auto& content : biomeContent()) {
            put(content.block,static_cast<std::uint8_t>(content.block),content.top,content.side,content.bottom,content.item);
            auto& definition=result[static_cast<std::size_t>(content.block)];
            const auto material=content.material;
            definition.hardness=material==BiomeMaterial::Wood?1.0f:material==BiomeMaterial::Plant?.05f:
                material==BiomeMaterial::Foliage?.15f:material==BiomeMaterial::Stone?1.25f:.5f;
            definition.correctTool=material==BiomeMaterial::Wood?ToolKind::Axe:
                material==BiomeMaterial::Ground?ToolKind::Shovel:
                material==BiomeMaterial::Stone||material==BiomeMaterial::Ice?ToolKind::Pickaxe:ToolKind::None;
            definition.soundMaterial=material==BiomeMaterial::Wood?SoundMaterial::Wood:
                material==BiomeMaterial::Ground||material==BiomeMaterial::Foliage||material==BiomeMaterial::Plant?
                    SoundMaterial::Grass:SoundMaterial::Stone;
        }
        put(Block::Air, 0, 3, 3, 3, Item::None);
        put(Block::Grass, 1, 0, 1, 2, Item::Grass);
        put(Block::Dirt, 2, 2, 2, 2, Item::Dirt);
        put(Block::Stone, 3, 3, 3, 3, Item::Cobblestone);
        put(Block::Sand, 4, 4, 4, 4, Item::Sand);
        put(Block::Log, 5, 6, 5, 6, Item::Log);
        put(Block::Leaves, 6, 7, 7, 7, Item::Leaves);
        put(Block::Water, 7, 8, 8, 8, Item::Water);
        put(Block::CoalOre, 8, 9, 9, 9, Item::Coal);
        put(Block::IronOre, 9, 10, 10, 10, Item::IronOre);
        put(Block::GoldOre, 10, 11, 11, 11, Item::GoldOre);
        put(Block::CopperOre, 11, 12, 12, 12, Item::CopperOre);
        put(Block::DiamondOre, 12, 13, 13, 13, Item::Diamond);
        put(Block::Planks, 13, 14, 14, 14, Item::Planks);
        put(Block::CraftingTable, 14, 15, 15, 15, Item::CraftingTable);
        put(Block::Torch, 15, 16, 16, 16, Item::Torch);
        put(Block::Farmland, 16, 2, 2, 2, Item::Dirt);
        put(Block::Crop0, 17, 7, 7, 7, Item::Seeds);
        put(Block::Crop1, 18, 7, 7, 7, Item::Seeds);
        put(Block::Crop2, 19, 7, 7, 7, Item::Seeds);
        put(Block::Crop3, 20, 7, 7, 7, Item::Seeds);
        put(Block::WaterFlow1, 21, 8, 8, 8, Item::Water);
        put(Block::WaterFlow2, 22, 8, 8, 8, Item::Water);
        put(Block::WaterFlow3, 23, 8, 8, 8, Item::Water);
        put(Block::WaterFlow4, 24, 8, 8, 8, Item::Water);
        put(Block::WaterFlow5, 25, 8, 8, 8, Item::Water);
        put(Block::WaterFlow6, 26, 8, 8, 8, Item::Water);
        put(Block::WaterFlow7, 27, 8, 8, 8, Item::Water);
        put(Block::WaterFalling, 28, 8, 8, 8, Item::Water);
        put(Block::Cobblestone, 29, 17, 17, 17, Item::Cobblestone);
        put(Block::StoneBricks, 30, 18, 18, 18, Item::StoneBricks);
        put(Block::Bricks, 31, 19, 19, 19, Item::Bricks);
        put(Block::Glass, 32, 20, 20, 20, Item::Glass);
        put(Block::Gravel, 33, 21, 21, 21, Item::Gravel);
        put(Block::Clay, 34, 22, 22, 22, Item::Clay);
        put(Block::Snow, 35, 23, 23, 23, Item::Snow);
        put(Block::SnowBlock, 36, 23, 23, 23, Item::SnowBlock);
        put(Block::BirchPlanks, 37, 24, 24, 24, Item::BirchPlanks);
        put(Block::BirchLog, 38, 26, 25, 26, Item::BirchLog);
        put(Block::BirchLeaves, 39, 27, 27, 27, Item::BirchLeaves);
        put(Block::Cactus, 40, 29, 28, 29, Item::Cactus);
        put(Block::Furnace, 41, 30, 30, 30, Item::Furnace);
        put(Block::Bookshelf, 42, 31, 31, 31, Item::Bookshelf);
        put(Block::WoodenDoor, 43, 32, 32, 32, Item::WoodenDoor);
        put(Block::WoodenSlab, 44, 14, 14, 14, Item::WoodenSlab);
        put(Block::StoneSlab, 45, 3, 3, 3, Item::StoneSlab);
        put(Block::Granite, 46, 33, 33, 33, Item::Granite);
        put(Block::Diorite, 47, 34, 34, 34, Item::Diorite);
        put(Block::Andesite, 48, 35, 35, 35, Item::Andesite);
        put(Block::MossyCobblestone, 49, 36, 36, 36, Item::MossyCobblestone);
        put(Block::MossyStoneBricks, 50, 37, 37, 37, Item::MossyStoneBricks);
        put(Block::Ice, 51, 38, 38, 38, Item::Ice);
        put(Block::Mud, 52, 39, 39, 39, Item::Mud);
        put(Block::TallGrass, 53, 40, 40, 40, Item::TallGrass);
        put(Block::RedFlower, 54, 41, 41, 41, Item::RedFlower);
        put(Block::YellowFlower, 55, 42, 42, 42, Item::YellowFlower);
        put(Block::LadderNorth, 56, 43, 43, 43, Item::Ladder);
        put(Block::LadderSouth, 57, 3, 3, 3, Item::Ladder);
        put(Block::LadderEast, 58, 3, 3, 3, Item::Ladder);
        put(Block::LadderWest, 59, 3, 3, 3, Item::Ladder);
        put(Block::Chest, 60, 44, 44, 44, Item::Chest);
        put(Block::WoodenSlabTop, 61, 14, 14, 14, Item::WoodenSlab);
        put(Block::StoneSlabTop, 62, 3, 3, 3, Item::StoneSlab);
        put(Block::DoorClosedNorthLower, 63, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedNorthUpper, 64, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedSouthLower, 65, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedSouthUpper, 66, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedEastLower, 67, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedEastUpper, 68, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedWestLower, 69, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorClosedWestUpper, 70, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenNorthLower, 71, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenNorthUpper, 72, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenSouthLower, 73, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenSouthUpper, 74, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenEastLower, 75, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenEastUpper, 76, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenWestLower, 77, 32, 32, 32, Item::WoodenDoor);
        put(Block::DoorOpenWestUpper, 78, 32, 32, 32, Item::WoodenDoor);
        for (int offset = 0; offset < 16; ++offset) {
            const Block state = static_cast<Block>(
                static_cast<int>(Block::DoorRightClosedNorthLower) + offset);
            put(state, static_cast<std::uint8_t>(79 + offset),
                32, 32, 32, Item::WoodenDoor);
        }
        put(Block::CoalBlock, 95, 45, 45, 45, Item::CoalBlock);
        put(Block::IronBlock, 96, 46, 46, 46, Item::IronBlock);
        put(Block::GoldBlock, 97, 47, 47, 47, Item::GoldBlock);
        put(Block::CopperBlock, 98, 48, 48, 48, Item::CopperBlock);
        put(Block::DiamondBlock, 99, 49, 49, 49, Item::DiamondBlock);
        put(Block::HayBale, 100, 51, 50, 51, Item::HayBale);
        put(Block::Sandstone, 101, 53, 52, 53, Item::Sandstone);
        put(Block::PolishedGranite, 102, 54, 54, 54, Item::PolishedGranite);
        put(Block::PolishedDiorite, 103, 55, 55, 55, Item::PolishedDiorite);
        put(Block::PolishedAndesite, 104, 56, 56, 56, Item::PolishedAndesite);
        put(Block::SugarCane, 105, 57, 57, 57, Item::SugarCane);
        for (int offset = 0; offset < 4; ++offset)
            put(static_cast<Block>(static_cast<int>(Block::VineNorth) + offset),
                static_cast<std::uint8_t>(106 + offset), 58, 58, 58, Item::Vine);
        put(Block::LightningRod, 110, 48, 48, 48, Item::LightningRod);
        put(Block::WetFarmland, 111, 2, 2, 2, Item::Dirt);
        put(Block::Fire, 112, 59, 59, 59, Item::None);
        result[static_cast<std::size_t>(Block::Fire)].emittedLight = 15;
        for(int layer=2;layer<=8;++layer) {
            const Block block=snowLayerBlock(layer);
            put(block,static_cast<std::uint8_t>(block),23,23,23,Item::Snowball);
            auto& definition=result[static_cast<std::size_t>(block)];
            definition.dropCount=layer;
            definition.soundMaterial=SoundMaterial::Snow;
            definition.hardness=.15f;
        }
        result[static_cast<std::size_t>(Block::Clay)].drop = Item::ClayBall;
        result[static_cast<std::size_t>(Block::Clay)].dropCount = 4;
        result[static_cast<std::size_t>(Block::Snow)].drop = Item::Snowball;
        auto configure = [&](Block block, float hardness, ToolKind tool = ToolKind::None,
                             int harvestTier = 0) {
            BlockDefinition& definition = result[static_cast<std::size_t>(block)];
            definition.hardness = hardness;
            definition.correctTool = tool;
            definition.requiredHarvestTier = harvestTier;
        };
        for (Block block : {Block::Leaves, Block::BirchLeaves, Block::Snow, Block::Torch,
                            Block::TallGrass, Block::RedFlower, Block::YellowFlower,
                            Block::LadderNorth, Block::LadderSouth, Block::LadderEast,
                            Block::LadderWest})
            configure(block, .15f);
        configure(Block::Glass, .25f);
        for (Block block : {Block::Dirt, Block::Grass, Block::Sand, Block::Gravel,
                            Block::Clay, Block::SnowBlock, Block::Mud})
            configure(block, .45f, ToolKind::Shovel);
        for (Block block : {Block::Log, Block::BirchLog, Block::Planks, Block::BirchPlanks,
                            Block::CraftingTable, Block::Bookshelf, Block::WoodenDoor,
                            Block::WoodenSlab, Block::WoodenSlabTop, Block::Cactus,
                            Block::Chest})
            configure(block, .9f, ToolKind::Axe);
        for (Block block : {Block::Stone, Block::Cobblestone, Block::StoneBricks,
                            Block::Bricks, Block::Furnace, Block::StoneSlab,
                            Block::StoneSlabTop, Block::Granite, Block::Diorite,
                            Block::Andesite, Block::MossyCobblestone,
                            Block::MossyStoneBricks, Block::Ice})
            configure(block, 1.5f, ToolKind::Pickaxe);
        configure(Block::CoalOre, 2.0f, ToolKind::Pickaxe, 1);
        configure(Block::CopperOre, 2.0f, ToolKind::Pickaxe, 2);
        configure(Block::IronOre, 2.5f, ToolKind::Pickaxe, 2);
        configure(Block::GoldOre, 3.0f, ToolKind::Pickaxe, 3);
        configure(Block::DiamondOre, 4.0f, ToolKind::Pickaxe, 3);
        for (int id = static_cast<int>(Block::DoorClosedNorthLower);
             id <= static_cast<int>(Block::DoorRightOpenWestUpper); ++id)
            configure(static_cast<Block>(id), .9f, ToolKind::Axe);
        auto material = [&](SoundMaterial sound, std::initializer_list<Block> blocks) {
            for (Block block : blocks)
                result[static_cast<std::size_t>(block)].soundMaterial = sound;
        };
        material(SoundMaterial::Wood,
                 {Block::Log, Block::BirchLog, Block::Planks, Block::BirchPlanks,
                  Block::CraftingTable, Block::Chest, Block::Bookshelf,
                  Block::WoodenDoor, Block::WoodenSlab, Block::WoodenSlabTop,
                  Block::LadderNorth, Block::LadderSouth, Block::LadderEast,
                  Block::LadderWest, Block::Torch});
        for (int id = static_cast<int>(Block::DoorClosedNorthLower);
             id <= static_cast<int>(Block::DoorRightOpenWestUpper); ++id)
            result[static_cast<std::size_t>(id)].soundMaterial = SoundMaterial::Wood;
        material(SoundMaterial::Grass,
                 {Block::Grass, Block::Dirt, Block::Farmland, Block::Mud,
                  Block::Leaves, Block::BirchLeaves, Block::Cactus,
                  Block::TallGrass, Block::RedFlower, Block::YellowFlower,
                  Block::Crop0, Block::Crop1, Block::Crop2, Block::Crop3});
        material(SoundMaterial::Gravel, {Block::Gravel, Block::Clay});
        material(SoundMaterial::Sand, {Block::Sand});
        material(SoundMaterial::Snow, {Block::Snow, Block::SnowBlock});
        for (Block block : {Block::Farmland, Block::Crop0, Block::Crop1, Block::Crop2,
                            Block::Crop3, Block::WaterFlow1, Block::WaterFlow2,
                            Block::WaterFlow3, Block::WaterFlow4, Block::WaterFlow5,
                            Block::WaterFlow6, Block::WaterFlow7, Block::WaterFalling})
            result[static_cast<std::size_t>(block)].drop = Item::None;
        result[static_cast<std::size_t>(Block::Torch)].emittedLight = 15;
        for (Block block : {Block::CoalBlock, Block::IronBlock, Block::GoldBlock,
                            Block::CopperBlock, Block::DiamondBlock})
            configure(block, 3.0f, ToolKind::Pickaxe,
                      block == Block::GoldBlock || block == Block::DiamondBlock ? 3 :
                      block == Block::CoalBlock ? 1 : 2);
        for (Block block : {Block::Sandstone, Block::PolishedGranite,
                            Block::PolishedDiorite, Block::PolishedAndesite})
            configure(block, 1.5f, ToolKind::Pickaxe);
        configure(Block::HayBale, .5f);
        configure(Block::LightningRod, 1.5f, ToolKind::Pickaxe, 1);
        material(SoundMaterial::Grass,{Block::WetFarmland});
        material(SoundMaterial::Grass, {Block::HayBale, Block::SugarCane,
                 Block::VineNorth, Block::VineSouth, Block::VineEast, Block::VineWest});
        material(SoundMaterial::Sand, {Block::Sandstone});
        for (Block block : {Block::SugarCane, Block::VineNorth, Block::VineSouth,
                            Block::VineEast, Block::VineWest})
            configure(block, .15f);
        return result;
    }();
    return definitions;
}
} // namespace

const ItemDefinition& itemDefinition(Item item) {
    const std::size_t index = static_cast<std::size_t>(item);
    return items()[index < items().size() ? index : 0];
}
const BlockDefinition& blockDefinition(Block block) {
    const std::size_t index = static_cast<std::size_t>(block);
    return blocks()[index < blocks().size() ? index : 0];
}
bool itemFromSaveId(std::uint8_t id, Item& item) {
    static const auto ids = [] {
        std::array<Item, 256> lookup{};
        lookup.fill(Item::Count);
        for (std::size_t index = 0; index < items().size(); ++index)
            lookup[items()[index].saveId] = static_cast<Item>(index);
        return lookup;
    }();
    item = ids[id];
    return item != Item::Count;
}
bool blockFromSaveId(std::uint8_t id, Block& block) {
    static const auto ids = [] {
        std::array<Block, 256> lookup{};
        lookup.fill(Block::Count);
        for (std::size_t index = 0; index < blocks().size(); ++index)
            lookup[blocks()[index].saveId] = static_cast<Block>(index);
        return lookup;
    }();
    block = ids[id];
    return block != Block::Count;
}
int blockTexture(Block block, int faceIndex) {
    const BlockDefinition& definition = blockDefinition(block);
    return faceIndex == 2 ? definition.topTexture
         : faceIndex == 3 ? definition.bottomTexture : definition.sideTexture;
}
float furnaceFuelSeconds(Item item) { return itemDefinition(item).fuelSeconds; }
ItemAtlasLayout& itemAtlasLayout() {
    static ItemAtlasLayout layout;
    return layout;
}
bool setItemAtlasDimensions(int width, int height, int tilePixels) {
    if (tilePixels <= 0 || width <= 0 || height <= 0 ||
        width % tilePixels != 0 || height % tilePixels != 0)
        return false;
    itemAtlasLayout() = {width, height, tilePixels, width / tilePixels,
                         height / tilePixels};
    return true;
}

bool itemSpriteUv(Item item, ItemSpriteUv& uv) {
    const ItemAtlasLayout& layout = itemAtlasLayout();
    const int index = itemDefinition(item).spriteIndex;
    if (index < 0 || layout.columns <= 0 || layout.rows <= 0 ||
        index >= layout.columns * layout.rows)
        return false;
    const int column = index % layout.columns;
    const int row = index / layout.columns;
    // The .rgba file stores its top image row first. OpenGL uploads that row
    // at texture v=0, matching the UI's top vertices.
    // Sample inside the cell; nearest filtering and transparent sprite gutters
    // prevent adjacent inventory icons from appearing along scaled edges.
    const float insetU = 0.5f / layout.width;
    const float insetV = 0.5f / layout.height;
    uv = {static_cast<float>(column) / layout.columns + insetU,
          static_cast<float>(row) / layout.rows + insetV,
          static_cast<float>(column + 1) / layout.columns - insetU,
          static_cast<float>(row + 1) / layout.rows - insetV};
    return true;
}
