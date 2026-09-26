#pragma once

#include "Block.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

class Player;
class World;

enum class Item : std::uint8_t {
    None = 0,
    Grass, Dirt, Stone, Sand, Log, Leaves, Water,
    CoalOre, IronOre, GoldOre, CopperOre, DiamondOre,
    Planks, Stick, CraftingTable, Torch,
    WoodPickaxe, StonePickaxe, IronPickaxe,
    WoodAxe, StoneAxe, IronAxe,
    WoodShovel, StoneShovel, IronShovel,
    Count
};

struct ItemStack {
    Item item = Item::None;
};

struct Recipe {
    std::string name;
    std::vector<std::pair<Item, int>> ingredients;
    Item output = Item::None;
    int outputCount = 1;
    bool requiresTable = false;
};

class Inventory {
public:
    Inventory();

    int selectedSlot() const { return selectedSlot_; }
    void selectSlot(int slot);
    void cycleSlot(int direction);
    Item selectedItem() const;
    Item slotItem(int slot) const;
    int count(Item item) const;
    bool add(Item item, int amount = 1);
    bool remove(Item item, int amount = 1);
    bool craft(std::size_t recipeIndex, bool nearCraftingTable);

    const std::vector<Recipe>& recipes() const { return recipes_; }
    std::string recipeStatus(std::size_t recipeIndex, bool nearCraftingTable) const;

private:
    std::array<ItemStack, 9> hotbar_{};
    std::array<int, static_cast<std::size_t>(Item::Count)> counts_{};
    std::vector<Recipe> recipes_;
    int selectedSlot_ = 0;

    bool canCraft(const Recipe& recipe, bool nearCraftingTable) const;
};

Block itemToBlock(Item item);
Item blockToItem(Block block);
bool isTool(Item item);
float blockHardness(Block block);
float toolBreakMultiplier(Item item, Block block);
glm::vec3 itemColor(Item item);

struct RenderCuboid {
    glm::vec3 center{0.0f};
    glm::vec3 size{1.0f};
    glm::vec3 color{1.0f};
};

class SurvivalWorld {
public:
    explicit SurvivalWorld(std::uint32_t seed);

    void update(float deltaTime, World& world, Player& player, Inventory& inventory, float daylight);
    void spawnDrop(const glm::vec3& position, Item item, int amount = 1);
    std::vector<RenderCuboid> renderCuboids() const;

private:
    enum class AnimalType : std::uint8_t { Cow, Pig, Sheep, Wolf };
    struct Animal {
        AnimalType type = AnimalType::Cow;
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec2 heading{1.0f, 0.0f};
        float thinkTimer = 0.0f;
        float damageCooldown = 0.0f;
        bool grounded = false;
    };
    struct Drop {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        Item item = Item::None;
        int amount = 1;
        float age = 0.0f;
    };

    std::uint32_t seed_ = 0;
    std::vector<Animal> animals_;
    std::vector<Drop> drops_;
    std::unordered_set<std::int64_t> spawnedChunks_;

    void spawnNearbyAnimals(World& world, const glm::vec3& playerPosition);
    void updateAnimal(Animal& animal, float deltaTime, World& world, Player& player, float daylight);
    static std::int64_t chunkKey(int x, int z);
};

