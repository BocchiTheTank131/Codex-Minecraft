#pragma once

#include "Block.h"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_set>
#include <vector>

class Player;
class SoundSystem;
class World;

// Never reorder existing values: inventory V4 saves store these numeric IDs.
enum class Item : std::uint8_t {
    None = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Log,
    Leaves,
    Water,
    CoalOre,
    IronOre,
    GoldOre,
    CopperOre,
    DiamondOre,
    Planks,
    Stick,
    CraftingTable,
    Torch,
    WoodPickaxe,
    StonePickaxe,
    IronPickaxe,
    WoodAxe,
    StoneAxe,
    IronAxe,
    WoodShovel,
    StoneShovel,
    IronShovel,
    Seeds,
    Wheat,
    Bread,
    RawMeat,
    RawBeef,
    RawPork,
    Wool,
    CookedBeef,
    CookedPork,
    Cobblestone,
    StoneBricks,
    Bricks,
    Glass,
    Gravel,
    Clay,
    Snow,
    SnowBlock,
    BirchPlanks,
    BirchLog,
    BirchLeaves,
    Cactus,
    Furnace,
    Bookshelf,
    WoodenDoor,
    WoodenSlab,
    StoneSlab,
    Coal,
    IronIngot,
    GoldIngot,
    Diamond,
    CopperIngot,
    Apple,
    Leather,
    RawMutton,
    CookedMutton,
    Granite,
    Diorite,
    Andesite,
    MossyCobblestone,
    MossyStoneBricks,
    Ice,
    Mud,
    TallGrass,
    RedFlower,
    YellowFlower,
    Ladder,
    Chest,
    GoldPickaxe,
    GoldAxe,
    GoldShovel,
    DiamondPickaxe,
    DiamondAxe,
    DiamondShovel,
    WoodSword,
    StoneSword,
    IronSword,
    GoldSword,
    DiamondSword,
    Paper,
    Book,
    ClayBall,
    Brick,
    Snowball,
    Charcoal,
    SugarCane,
    Vine,
    CoalBlock,
    IronBlock,
    GoldBlock,
    CopperBlock,
    DiamondBlock,
    HayBale,
    Sandstone,
    PolishedGranite,
    PolishedDiorite,
    PolishedAndesite,
    Count
};

struct ItemStack {
    Item item = Item::None;
    int count = 0;
    int durability = 0;
    bool empty() const {
        return item == Item::None || count <= 0;
    }
    void clear() {
        item = Item::None;
        count = 0;
        durability = 0;
    }
};

enum class RecipeCategory : int { All, Building, Tools, Food, Misc, Count };
enum class IngredientGroup : std::uint8_t { None, Planks, Logs };
struct Ingredient {
    Item item = Item::None;
    IngredientGroup group = IngredientGroup::None;
    Ingredient() = default;
    Ingredient(Item value) : item(value) {}
    Ingredient(IngredientGroup value) : group(value) {}
    bool empty() const { return item == Item::None && group == IngredientGroup::None; }
    bool accepts(Item value) const {
        if (group == IngredientGroup::Planks)
            return value == Item::Planks || value == Item::BirchPlanks;
        if (group == IngredientGroup::Logs)
            return value == Item::Log || value == Item::BirchLog;
        return item == value;
    }
};
struct RecipeInfo {
    int w = 0;
    int h = 0;
    std::array<Ingredient, 9> cells{};
    Item output = Item::None;
    int count = 0;
    bool table = false;
    bool allowMirror = false;
    bool shapeless = false;
    RecipeCategory category = RecipeCategory::Misc;
};
const std::vector<RecipeInfo>& craftingRecipes();

class Inventory {
public:
    static constexpr int HotbarSlots = 9, TotalSlots = 36, CraftSlots = 9;
    Inventory();
    int selectedSlot() const {
        return selectedSlot_;
    }
    void selectSlot(int slot);
    void cycleSlot(int direction);
    bool pickBlock(Item item, bool creativeMode);
    Item selectedItem() const;
    const ItemStack& selectedStack() const;
    const ItemStack& slot(int index) const;
    const ItemStack& craftSlot(int index) const;
    const ItemStack& cursorStack() const {
        return cursor_;
    }
    int count(Item item) const;
    int add(Item item, int amount = 1, int durability = -1);
    bool remove(Item item, int amount = 1);
    bool consumeSelected(int amount = 1);
    bool damageSelectedTool(int amount = 1);
    void clear();
    void setCreativeCursor(Item item);
    bool handleCreativeClick(
        double mouseX, double mouseY, int width, int height, bool rightClick);
    ItemStack craftingOutput(bool tableMode) const;
    bool takeCraftingOutput(bool tableMode);
    int craftOutputToInventory(bool tableMode);
    bool recipeCraftable(int recipeIndex, bool tableMode) const;
    bool fillRecipe(int recipeIndex, bool tableMode, bool maximize);
    bool shiftIngredientToCrafting(int playerSlot, int recipeIndex, bool tableMode);
    bool gatherMatchingToCursor();
    bool handleInventoryClick(double mouseX,
                              double mouseY,
                              int width,
                              int height,
                              bool rightClick,
                              bool tableMode,
                              bool shiftClick = false);
    bool clickPlayerSlot(int index, bool rightClick);
    bool clickExternalSlot(ItemStack& stack, bool rightClick, bool outputOnly = false);
    bool moveExternalToInventory(ItemStack& stack);
    bool movePlayerToExternal(int index, ItemStack* stacks, int stackCount);
    ItemStack takeCursor(bool singleItem);
    ItemStack takeSelected(bool entireStack);
    void returnCraftingItems();
    bool load(const std::string& path, std::uint32_t seed);
    bool save(const std::string& path, std::uint32_t seed) const;
    static int maxStack(Item item);
    static int maxDurability(Item item);
    static bool runCraftingSelfTest(std::string& report);

private:
    std::array<ItemStack, TotalSlots> slots_{};
    std::array<ItemStack, CraftSlots> crafting_{};
    ItemStack cursor_{};
    int selectedSlot_ = 0;
    static void normalize(ItemStack& stack);
    static bool canMerge(const ItemStack& a, const ItemStack& b);
    static void clickStack(ItemStack& slot, ItemStack& cursor, bool rightClick);
    bool findCraftingMatch(bool tableMode, ItemStack& output, std::vector<int>* consumed) const;
};

Block itemToBlock(Item item);
Item blockToItem(Block block);
const std::vector<Item>& creativeCatalog();
bool isTool(Item item);
bool isSword(Item item);
bool canHarvestBlock(Item item, Block block);
Item smeltingResult(Item item);
bool isFood(Item item);
float foodValue(Item item);
float blockHardness(Block block);
float toolBreakMultiplier(Item item, Block block);
float attackDamage(Item item);
float attackCooldown(Item item);
glm::vec3 itemColor(Item item);

struct RenderItemSprite {
    glm::vec3 center{0};
    Item item = Item::None;
    float size = .42f;
};
struct RenderCuboid {
    glm::vec3 center{0};
    glm::vec3 size{1};
    glm::vec3 color{1};
    glm::vec3 direction{0};
};
struct RenderBillboard {
    glm::vec3 feet{0};
    std::uint8_t variant = 0;
    float hurt = 0;
    float opacity = 1;
};
inline constexpr float BillboardMobHeight = 1.75f;
inline constexpr float BillboardMobHalfWidth = 0.355f;
struct MobTarget {
    int index = -1;
    float distance = 0;
    glm::vec3 center{0};
    bool valid() const {
        return index >= 0;
    }
};
struct AttackResult {
    bool hit = false, killed = false, critical = false;
    float damage = 0;
    glm::vec3 position{0};
};

class SurvivalWorld {
public:
    struct MobDiagnostics {
        int passive = 0;
        int hostile = 0;
        int passiveCap = 0;
        int hostileCap = 0;
        int spawnAttempts = 0;
        int spawnSuccesses = 0;
        int navigationQueries = 0;
        float aiMilliseconds = 0.0f;
        int arrows = 0;
    };
    explicit SurvivalWorld(std::uint32_t seed);
    void setSoundSystem(SoundSystem* sounds) { sounds_ = sounds; }
    void
    update(float deltaTime, World& world, Player& player, Inventory& inventory, float daylight);
    void spawnDrop(const glm::vec3& position,
                   Item item,
                   int amount = 1,
                   int durability = -1,
                   const glm::vec3& velocity = glm::vec3(0.4f, 0.8f, 0.2f),
                   float pickupDelay = 0.0f);
    void spawnExperience(const glm::vec3& position, int amount);
    void spawnStructureMob(const glm::ivec3& position, bool hostile);
    void spawnBillboardPreview(const glm::vec3& origin, const glm::vec3& forward);
    bool summonMob(const std::string& name, const glm::vec3& position);
    bool feedAnimal(const glm::vec3& origin, const glm::vec3& direction, Item food);
    MobTarget raycastMob(const glm::vec3& origin,
                         const glm::vec3& direction,
                         float reach,
                         const World& world,
                         float blockDistance) const;
    AttackResult
    attackMob(int index, Item heldItem, bool critical, const glm::vec3& attackerPosition);
    RenderCuboid mobOutline(int index) const;
    std::vector<RenderCuboid> renderCuboids() const;
    std::vector<RenderBillboard> renderBillboards() const;
    void renderCuboids(std::vector<RenderCuboid>& out) const;
    void renderBillboards(std::vector<RenderBillboard>& out) const;
    std::string mobName(int index) const;
    std::vector<RenderItemSprite> renderItemSprites() const;
    void renderItemSprites(std::vector<RenderItemSprite>& out) const;
    std::vector<glm::vec3> takeExplosionEffects();
    bool save(const std::string& path, std::uint32_t seed) const;
    bool load(const std::string& path, std::uint32_t seed);
    bool runCombatSelfTest(World& world, Player& player, Inventory& inventory, std::string& report);
    MobDiagnostics diagnostics(const glm::vec3& playerPosition) const;

private:
    // ID 3 is a retired Wolf ID in old save files. Never reuse it.
    enum class AnimalType : std::uint8_t { Cow = 0, Pig = 1, Sheep = 2, Villager = 4,
                                            Pillager = 5, BillboardFirst = 6 };
    static bool isBillboard(AnimalType type);
    static std::size_t billboardIndex(AnimalType type);
    static AnimalType billboardType(std::size_t index);
    struct Animal {
        AnimalType type = AnimalType::Cow;
        glm::vec3 position{0}, velocity{0}, home{0};
        glm::vec2 heading{1, 0};
        float thinkTimer = 0, attackCooldown = 0, hurtCooldown = 0, hurtFlash = 0, loveTimer = 0,
              breedingCooldown = 0, age = 0, health = 10, deathTimer = 0;
        float simulationAccumulator = 0;
        float ambientSoundTimer = 3.0f;
        float stepSoundTimer = 0.0f;
        float fuseTimer = 0.0f;
        float lungeCooldown = 0.0f;
        float stepJumpCooldown = 0.0f;
        float fleeTimer = 0, angerTimer = 0, memoryTimer = 0, idleTimer = 0,
              wanderPhase = 0;
        glm::vec3 rememberedTarget{0};
        bool persistent = false;
        bool grounded = false, dropsReleased = false;
    };
    struct Drop {
        glm::vec3 position{0}, velocity{0};
        ItemStack stack{};
        float age = 0;
        float pickupDelay = 0;
        float simulationAccumulator = 0;
    };
    struct ExperienceOrb {
        glm::vec3 position{0}, velocity{0};
        int value = 1;
        float age = 0;
        float simulationAccumulator = 0;
    };
    struct Arrow {
        glm::vec3 position{0}, velocity{0};
        float age = 0.0f;
        bool active = true;
        std::uint8_t sourceType = 0;
    };
    std::uint32_t seed_ = 0;
    SoundSystem* sounds_ = nullptr;
    std::vector<Animal> animals_;
    std::vector<Drop> drops_;
    std::vector<ExperienceOrb> experienceOrbs_;
    std::vector<Arrow> arrows_;
    std::vector<glm::vec3> explosionEffects_;
    std::unordered_set<std::int64_t> spawnedChunks_;
    std::unordered_set<std::uint64_t> spawnedStructureMarkers_;
    float spawnAccumulator_ = 0.0f;
    float passiveSpawnCooldown_ = 0.0f;
    std::uint32_t spawnSequence_ = 0;
    int spawnAttemptsLastTick_ = 0;
    int spawnSuccessesLastTick_ = 0;
    int navigationQueriesLastTick_ = 0;
    float aiMilliseconds_ = 0.0f;
    glm::vec3 lastActivePlayerPosition_{0.0f};
    bool hasActivePlayerPosition_ = false;
    void spawnNearbyAnimals(World& world, const glm::vec3& playerPosition, float daylight);
    void
    updateAnimal(Animal& animal, float deltaTime, World& world, Player& player,
                 const Inventory& inventory, float daylight);
    bool canNavigateTo(const Animal& animal, const World& world,
                       const glm::vec2& direction) const;
    glm::vec2 chooseNavigationHeading(Animal& animal, World& world,
                                      const glm::vec2& desired);
    bool canSeePlayer(const Animal& animal, const World& world,
                      const Player& player, float maximumDistance = 16.0f) const;
    void detonate(const glm::vec3& center, World& world, Player& player);
    void updateArrows(float deltaTime, World& world, Player& player);
    int targetedAnimal(const glm::vec3& origin, const glm::vec3& direction, float reach) const;
    void releaseDrops(Animal& animal);
    static std::int64_t chunkKey(int x, int z);
};
