#pragma once

#include "Survival.h"
#include <cstdint>
#include <string>

enum class ToolKind : std::uint8_t { None, Pickaxe, Axe, Shovel, Sword };
enum class SoundMaterial : std::uint8_t { Stone, Wood, Grass, Gravel, Sand, Snow };

struct ItemDefinition {
    std::string displayName;
    std::uint8_t saveId = 0;
    int spriteIndex = -1;
    int maxStack = 64;
    ToolKind tool = ToolKind::None;
    int tier = 0;
    int durability = 0;
    float foodValue = 0.0f;
    float attackDamage = 1.0f;
    float attackCooldown = 0.42f;
    Block placedBlock = Block::Air;
    Item smeltedItem = Item::None;
    float fuelSeconds = 0.0f;
};

struct BlockDefinition {
    std::uint8_t saveId = 0;
    int topTexture = 3;
    int sideTexture = 3;
    int bottomTexture = 3;
    float hardness = 0.2f;
    ToolKind correctTool = ToolKind::None;
    int requiredHarvestTier = 0;
    Item drop = Item::None;
    int dropCount = 1;
    bool solid = false;
    bool transparent = false;
    BlockGeometryProperties geometry{};
    std::uint8_t emittedLight = 0;
    SoundMaterial soundMaterial = SoundMaterial::Stone;
};

const ItemDefinition& itemDefinition(Item item);
const BlockDefinition& blockDefinition(Block block);
bool itemFromSaveId(std::uint8_t id, Item& item);
bool blockFromSaveId(std::uint8_t id, Block& block);
int blockTexture(Block block, int faceIndex);
inline constexpr int BlockAtlasTiles = 96;
float furnaceFuelSeconds(Item item);

struct ItemAtlasLayout {
    int width = 640;
    int height = 640;
    int tilePixels = 64;
    int columns = 10;
    int rows = 10;
};
ItemAtlasLayout& itemAtlasLayout();
bool setItemAtlasDimensions(int width, int height, int tilePixels = 64);

struct ItemSpriteUv {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
};
bool itemSpriteUv(Item item, ItemSpriteUv& uv);
