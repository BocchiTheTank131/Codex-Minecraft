#pragma once

#include "Block.h"
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

class Player;
class World;

// Never reorder existing values: inventory V4 saves store these numeric IDs.
enum class Item : std::uint8_t {
    None=0, Grass,Dirt,Stone,Sand,Log,Leaves,Water,
    CoalOre,IronOre,GoldOre,CopperOre,DiamondOre,
    Planks,Stick,CraftingTable,Torch,
    WoodPickaxe,StonePickaxe,IronPickaxe,
    WoodAxe,StoneAxe,IronAxe,
    WoodShovel,StoneShovel,IronShovel,
    Seeds,Wheat,Bread,RawMeat,
    RawBeef,RawPork,Wool,CookedBeef,CookedPork,
    Count
};

struct ItemStack {
    Item item=Item::None; int count=0; int durability=0;
    bool empty()const{return item==Item::None||count<=0;}
    void clear(){item=Item::None;count=0;durability=0;}
};

class Inventory {
public:
    static constexpr int HotbarSlots=9,TotalSlots=36,CraftSlots=9;
    Inventory();
    int selectedSlot()const{return selectedSlot_;} void selectSlot(int slot); void cycleSlot(int direction);
    Item selectedItem()const; const ItemStack& selectedStack()const; const ItemStack& slot(int index)const;
    const ItemStack& craftSlot(int index)const; const ItemStack& cursorStack()const{return cursor_;}
    int count(Item item)const; int add(Item item,int amount=1,int durability=-1); bool remove(Item item,int amount=1);
    bool consumeSelected(int amount=1); bool damageSelectedTool(int amount=1);
    ItemStack craftingOutput(bool tableMode)const; bool takeCraftingOutput(bool tableMode);
    bool handleInventoryClick(double mouseX,double mouseY,int width,int height,bool rightClick,bool tableMode,bool shiftClick=false);
    ItemStack takeCursor(bool singleItem); void returnCraftingItems();
    bool load(const std::string& path,std::uint32_t seed); bool save(const std::string& path,std::uint32_t seed)const;
    static int maxStack(Item item); static int maxDurability(Item item);
    static bool runCraftingSelfTest(std::string& report);
private:
    std::array<ItemStack,TotalSlots> slots_{}; std::array<ItemStack,CraftSlots> crafting_{}; ItemStack cursor_{}; int selectedSlot_=0;
    static void normalize(ItemStack& stack); static bool canMerge(const ItemStack&a,const ItemStack&b);
    static void clickStack(ItemStack&slot,ItemStack&cursor,bool rightClick);
    bool findCraftingMatch(bool tableMode,ItemStack& output,std::vector<int>* consumed)const;
};

Block itemToBlock(Item item); Item blockToItem(Block block); bool isTool(Item item); bool isFood(Item item);
float foodValue(Item item); float blockHardness(Block block); float toolBreakMultiplier(Item item,Block block);
float attackDamage(Item item); float attackCooldown(Item item); glm::vec3 itemColor(Item item);

struct RenderItemSprite{glm::vec3 center{0};Item item=Item::None;float size=.42f;};
struct RenderCuboid{glm::vec3 center{0};glm::vec3 size{1};glm::vec3 color{1};};
struct MobTarget{int index=-1;float distance=0;glm::vec3 center{0};bool valid()const{return index>=0;}};
struct AttackResult{bool hit=false,killed=false,critical=false;float damage=0;glm::vec3 position{0};};

class SurvivalWorld {
public:
    explicit SurvivalWorld(std::uint32_t seed);
    void update(float deltaTime,World& world,Player& player,Inventory& inventory,float daylight);
    void spawnDrop(const glm::vec3& position,Item item,int amount=1,int durability=-1);
    void spawnExperience(const glm::vec3& position,int amount);
    bool feedAnimal(const glm::vec3& origin,const glm::vec3& direction,Item food);
    MobTarget raycastMob(const glm::vec3& origin,const glm::vec3& direction,float reach,const World& world,float blockDistance)const;
    AttackResult attackMob(int index,Item heldItem,bool critical,const glm::vec3& attackerPosition);
    RenderCuboid mobOutline(int index)const;
    std::vector<RenderCuboid> renderCuboids()const; std::vector<RenderItemSprite> renderItemSprites()const;
    bool save(const std::string& path,std::uint32_t seed)const; bool load(const std::string& path,std::uint32_t seed);
    bool runCombatSelfTest(World& world,Player& player,Inventory& inventory,std::string& report);
private:
    enum class AnimalType:std::uint8_t{Cow,Pig,Sheep,Wolf};
    struct Animal{AnimalType type=AnimalType::Cow;glm::vec3 position{0},velocity{0};glm::vec2 heading{1,0};float thinkTimer=0,attackCooldown=0,hurtCooldown=0,hurtFlash=0,loveTimer=0,breedingCooldown=0,age=0,health=10,deathTimer=0;bool grounded=false,dropsReleased=false;};
    struct Drop{glm::vec3 position{0},velocity{0};ItemStack stack{};float age=0;};
    struct ExperienceOrb{glm::vec3 position{0},velocity{0};int value=1;float age=0;};
    std::uint32_t seed_=0;std::vector<Animal> animals_;std::vector<Drop> drops_;std::vector<ExperienceOrb> experienceOrbs_;std::unordered_set<std::int64_t> spawnedChunks_;
    void spawnNearbyAnimals(World& world,const glm::vec3& playerPosition);
    void updateAnimal(Animal& animal,float deltaTime,World& world,Player& player,float daylight);
    int targetedAnimal(const glm::vec3& origin,const glm::vec3& direction,float reach)const;
    void releaseDrops(Animal& animal);
    static std::int64_t chunkKey(int x,int z);
};
