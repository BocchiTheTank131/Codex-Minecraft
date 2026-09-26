#include "Survival.h"

#include "Player.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>

namespace {
std::size_t itemIndex(Item item) { return static_cast<std::size_t>(item); }

std::uint32_t hashPosition(int x, int z, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8da6b343U;
    h ^= static_cast<std::uint32_t>(z) * 0xd8163841U;
    h ^= seed * 0xcb1ab31fU;
    h ^= h >> 13U; h *= 0x85ebca6bU;
    return h ^ (h >> 16U);
}

bool isPickaxe(Item item) { return item == Item::WoodPickaxe || item == Item::StonePickaxe || item == Item::IronPickaxe; }
bool isAxe(Item item) { return item == Item::WoodAxe || item == Item::StoneAxe || item == Item::IronAxe; }
bool isShovel(Item item) { return item == Item::WoodShovel || item == Item::StoneShovel || item == Item::IronShovel; }
float toolTier(Item item) {
    switch (item) {
        case Item::WoodPickaxe: case Item::WoodAxe: case Item::WoodShovel: return 2.0f;
        case Item::StonePickaxe: case Item::StoneAxe: case Item::StoneShovel: return 4.0f;
        case Item::IronPickaxe: case Item::IronAxe: case Item::IronShovel: return 6.5f;
        default: return 1.0f;
    }
}
}

Inventory::Inventory() {
    hotbar_ = {{{Item::Grass}, {Item::Stone}, {Item::Log}, {Item::Planks}, {Item::CraftingTable},
                {Item::Torch}, {Item::WoodPickaxe}, {Item::StonePickaxe}, {Item::IronPickaxe}}};
    counts_[itemIndex(Item::Grass)] = 12;
    counts_[itemIndex(Item::Log)] = 4;
    counts_[itemIndex(Item::Torch)] = 6;
    counts_[itemIndex(Item::WoodPickaxe)] = 1;

    recipes_ = {
        {"PLANKS  1 LOG -> 4", {{Item::Log,1}}, Item::Planks,4,false},
        {"STICKS  2 PLANKS -> 4", {{Item::Planks,2}}, Item::Stick,4,false},
        {"CRAFTING TABLE  4 PLANKS", {{Item::Planks,4}}, Item::CraftingTable,1,false},
        {"WOOD PICKAXE  3 PLANKS + 2 STICKS", {{Item::Planks,3},{Item::Stick,2}}, Item::WoodPickaxe,1,true},
        {"STONE PICKAXE  3 STONE + 2 STICKS", {{Item::Stone,3},{Item::Stick,2}}, Item::StonePickaxe,1,true},
        {"IRON PICKAXE  3 IRON + 2 STICKS", {{Item::IronOre,3},{Item::Stick,2}}, Item::IronPickaxe,1,true},
        {"WOOD AXE  3 PLANKS + 2 STICKS", {{Item::Planks,3},{Item::Stick,2}}, Item::WoodAxe,1,true},
        {"STONE AXE  3 STONE + 2 STICKS", {{Item::Stone,3},{Item::Stick,2}}, Item::StoneAxe,1,true},
        {"IRON AXE  3 IRON + 2 STICKS", {{Item::IronOre,3},{Item::Stick,2}}, Item::IronAxe,1,true},
        {"WOOD SHOVEL  1 PLANK + 2 STICKS", {{Item::Planks,1},{Item::Stick,2}}, Item::WoodShovel,1,true},
        {"STONE SHOVEL  1 STONE + 2 STICKS", {{Item::Stone,1},{Item::Stick,2}}, Item::StoneShovel,1,true},
        {"IRON SHOVEL  1 IRON + 2 STICKS", {{Item::IronOre,1},{Item::Stick,2}}, Item::IronShovel,1,true}
    };
}

void Inventory::selectSlot(int slot) { selectedSlot_ = std::clamp(slot, 0, 8); }
void Inventory::cycleSlot(int direction) { selectedSlot_ = (selectedSlot_ + direction + 9) % 9; }
Item Inventory::selectedItem() const { return hotbar_[static_cast<std::size_t>(selectedSlot_)].item; }
Item Inventory::slotItem(int slot) const { return slot >= 0 && slot < 9 ? hotbar_[static_cast<std::size_t>(slot)].item : Item::None; }
int Inventory::count(Item item) const { return counts_[itemIndex(item)]; }
bool Inventory::add(Item item, int amount) { if (item == Item::None || amount <= 0) return false; counts_[itemIndex(item)] += amount; return true; }
bool Inventory::remove(Item item, int amount) { if (count(item) < amount || amount <= 0) return false; counts_[itemIndex(item)] -= amount; return true; }

bool Inventory::canCraft(const Recipe& recipe, bool nearTable) const {
    if (recipe.requiresTable && !nearTable) return false;
    for (const auto& ingredient : recipe.ingredients) if (count(ingredient.first) < ingredient.second) return false;
    return true;
}

bool Inventory::craft(std::size_t recipeIndex, bool nearTable) {
    if (recipeIndex >= recipes_.size() || !canCraft(recipes_[recipeIndex], nearTable)) return false;
    const Recipe& recipe = recipes_[recipeIndex];
    for (const auto& ingredient : recipe.ingredients) remove(ingredient.first, ingredient.second);
    add(recipe.output, recipe.outputCount);
    return true;
}

std::string Inventory::recipeStatus(std::size_t index, bool nearTable) const {
    if (index >= recipes_.size()) return {};
    const Recipe& recipe = recipes_[index];
    std::ostringstream text;
    text << "CRAFT " << (index + 1) << "/" << recipes_.size() << "  " << recipe.name << '\n';
    text << (canCraft(recipe, nearTable) ? "PRESS V TO CRAFT" : (recipe.requiresTable && !nearTable ? "CRAFTING TABLE REQUIRED" : "MISSING MATERIALS"));
    return text.str();
}

Block itemToBlock(Item item) {
    switch (item) {
        case Item::Grass:return Block::Grass; case Item::Dirt:return Block::Dirt; case Item::Stone:return Block::Stone;
        case Item::Sand:return Block::Sand; case Item::Log:return Block::Log; case Item::Leaves:return Block::Leaves;
        case Item::Water:return Block::Water; case Item::CoalOre:return Block::CoalOre; case Item::IronOre:return Block::IronOre;
        case Item::GoldOre:return Block::GoldOre; case Item::CopperOre:return Block::CopperOre; case Item::DiamondOre:return Block::DiamondOre;
        case Item::Planks:return Block::Planks; case Item::CraftingTable:return Block::CraftingTable; case Item::Torch:return Block::Torch;
        default:return Block::Air;
    }
}

Item blockToItem(Block block) {
    switch (block) {
        case Block::Grass:return Item::Grass; case Block::Dirt:return Item::Dirt; case Block::Stone:return Item::Stone;
        case Block::Sand:return Item::Sand; case Block::Log:return Item::Log; case Block::Leaves:return Item::Leaves;
        case Block::Water:return Item::Water; case Block::CoalOre:return Item::CoalOre; case Block::IronOre:return Item::IronOre;
        case Block::GoldOre:return Item::GoldOre; case Block::CopperOre:return Item::CopperOre; case Block::DiamondOre:return Item::DiamondOre;
        case Block::Planks:return Item::Planks; case Block::CraftingTable:return Item::CraftingTable; case Block::Torch:return Item::Torch;
        default:return Item::None;
    }
}

bool isTool(Item item) { return isPickaxe(item) || isAxe(item) || isShovel(item); }

float blockHardness(Block block) {
    switch (block) {
        case Block::Leaves: case Block::Torch:return 0.18f;
        case Block::Dirt: case Block::Grass: case Block::Sand:return 0.55f;
        case Block::Log: case Block::Planks: case Block::CraftingTable:return 1.25f;
        case Block::Stone: case Block::CoalOre: case Block::CopperOre:return 2.2f;
        case Block::IronOre: case Block::GoldOre:return 2.8f;
        case Block::DiamondOre:return 3.6f;
        default:return 0.5f;
    }
}

float toolBreakMultiplier(Item item, Block block) {
    const bool stoneLike = block == Block::Stone || block == Block::CoalOre || block == Block::IronOre ||
                           block == Block::GoldOre || block == Block::CopperOre || block == Block::DiamondOre;
    const bool woodLike = block == Block::Log || block == Block::Planks || block == Block::CraftingTable;
    const bool soilLike = block == Block::Grass || block == Block::Dirt || block == Block::Sand;
    if ((stoneLike && isPickaxe(item)) || (woodLike && isAxe(item)) || (soilLike && isShovel(item))) return toolTier(item);
    return 1.0f;
}

glm::vec3 itemColor(Item item) {
    switch (item) {
        case Item::Grass:return {0.30f,0.67f,0.20f}; case Item::Dirt:return {0.48f,0.31f,0.17f};
        case Item::Stone:return {0.50f,0.50f,0.52f}; case Item::Sand:return {0.82f,0.75f,0.47f};
        case Item::Log:return {0.42f,0.26f,0.12f}; case Item::Leaves:return {0.16f,0.48f,0.13f};
        case Item::Water:return {0.10f,0.38f,0.72f}; case Item::CoalOre:return {0.15f,0.15f,0.17f};
        case Item::IronOre:return {0.72f,0.44f,0.27f}; case Item::GoldOre:return {0.92f,0.70f,0.12f};
        case Item::CopperOre:return {0.72f,0.34f,0.16f}; case Item::DiamondOre:return {0.16f,0.82f,0.86f};
        case Item::Planks:return {0.67f,0.45f,0.22f}; case Item::CraftingTable:return {0.52f,0.30f,0.13f};
        case Item::Torch:return {1.0f,0.68f,0.16f}; case Item::Stick:return {0.48f,0.30f,0.13f};
        case Item::WoodPickaxe: case Item::WoodAxe: case Item::WoodShovel:return {0.52f,0.32f,0.15f};
        case Item::StonePickaxe: case Item::StoneAxe: case Item::StoneShovel:return {0.50f,0.52f,0.55f};
        case Item::IronPickaxe: case Item::IronAxe: case Item::IronShovel:return {0.78f,0.80f,0.83f};
        default:return {0.7f,0.7f,0.7f};
    }
}

SurvivalWorld::SurvivalWorld(std::uint32_t seed) : seed_(seed) {}

std::int64_t SurvivalWorld::chunkKey(int x, int z) {
    const std::uint64_t high=static_cast<std::uint64_t>(static_cast<std::uint32_t>(x))<<32U;
    return static_cast<std::int64_t>(high|static_cast<std::uint32_t>(z));
}

void SurvivalWorld::spawnNearbyAnimals(World& world, const glm::vec3& playerPosition) {
    const int centerX=static_cast<int>(std::floor(playerPosition.x/CHUNK_SIZE));
    const int centerZ=static_cast<int>(std::floor(playerPosition.z/CHUNK_SIZE));
    for(int dz=-4;dz<=4;++dz)for(int dx=-4;dx<=4;++dx){
        const int cx=centerX+dx,cz=centerZ+dz;const std::int64_t key=chunkKey(cx,cz);
        if(!spawnedChunks_.insert(key).second)continue;
        const std::uint32_t hash=hashPosition(cx,cz,seed_);const int count=static_cast<int>(hash%3U);
        for(int i=0;i<count && animals_.size()<56;++i){
            const int wx=cx*CHUNK_SIZE+2+static_cast<int>((hash>>(i*5U))%12U);
            const int wz=cz*CHUNK_SIZE+2+static_cast<int>((hash>>(i*7U+3U))%12U);
            const int y=world.terrainHeight(wx,wz)+1;const std::string biome=world.biomeNameAt(wx,wz);
            if(y<=SEA_LEVEL+1 || biome=="DESERT")continue;
            AnimalType type=AnimalType::Cow;
            if(biome=="FOREST")type=(hash&8U)?AnimalType::Pig:AnimalType::Wolf;
            else if(biome=="MOUNTAINS")type=AnimalType::Sheep;
            else type=(hash&16U)?AnimalType::Cow:AnimalType::Pig;
            const float angle=static_cast<float>(hash%628U)*0.01f;
            animals_.push_back({type,{wx+0.5f,static_cast<float>(y),wz+0.5f},{0,0,0},{std::cos(angle),std::sin(angle)},1.0f,0.0f,false});
        }
    }
}

void SurvivalWorld::updateAnimal(Animal& animal,float dt,World& world,Player& player,float daylight){
    animal.thinkTimer-=dt;animal.damageCooldown=std::max(0.0f,animal.damageCooldown-dt);
    const glm::vec3 playerDelta=player.position()-animal.position;const bool wolf=animal.type==AnimalType::Wolf;
    if(wolf && daylight<0.35f && glm::dot(playerDelta,playerDelta)<144.0f){glm::vec2 chase(playerDelta.x,playerDelta.z);if(glm::dot(chase,chase)>0.01f)animal.heading=glm::normalize(chase);animal.thinkTimer=0.5f;}
    else if(animal.thinkTimer<=0.0f){const std::uint32_t h=hashPosition(static_cast<int>(animal.position.x*7),static_cast<int>(animal.position.z*11),seed_+static_cast<std::uint32_t>(animals_.size()));const float a=static_cast<float>(h%628U)*0.01f;animal.heading={std::cos(a),std::sin(a)};animal.thinkTimer=1.5f+static_cast<float>((h>>8U)%40U)*0.1f;}
    const float speed=wolf&&daylight<0.35f?2.1f:1.15f;animal.velocity.x=animal.heading.x*speed;animal.velocity.z=animal.heading.y*speed;animal.velocity.y-=18.0f*dt;
    constexpr float half=0.38f,height=1.15f;
    auto collides=[&](const glm::vec3& p){return world.aabbIntersectsSolid(p+glm::vec3(-half,0,-half),p+glm::vec3(half,height,half));};
    for(int axis:{0,2}){glm::vec3 attempt=animal.position;attempt[axis]+=animal.velocity[axis]*dt;if(collides(attempt)){animal.velocity[axis]=0;animal.heading=-animal.heading;if(animal.grounded)animal.velocity.y=5.0f;}else animal.position=attempt;}
    glm::vec3 vertical=animal.position;vertical.y+=animal.velocity.y*dt;if(collides(vertical)){if(animal.velocity.y<0)animal.grounded=true;animal.velocity.y=0;}else{animal.position=vertical;animal.grounded=false;}
    if(wolf&&daylight<0.35f&&glm::dot(playerDelta,playerDelta)<1.35f*1.35f&&animal.damageCooldown<=0.0f){player.damage(2.0f);animal.damageCooldown=1.1f;}
}

void SurvivalWorld::update(float dt,World& world,Player& player,Inventory& inventory,float daylight){
    spawnNearbyAnimals(world,player.position());
    for(Animal& animal:animals_)updateAnimal(animal,dt,world,player,daylight);
    animals_.erase(std::remove_if(animals_.begin(),animals_.end(),[&](const Animal& a){glm::vec2 d(a.position.x-player.position().x,a.position.z-player.position().z);return glm::dot(d,d)>180.0f*180.0f||a.position.y<-20.0f;}),animals_.end());
    for(Drop& drop:drops_){drop.age+=dt;drop.velocity.y-=16.0f*dt;glm::vec3 next=drop.position+drop.velocity*dt;if(world.aabbIntersectsSolid(next-glm::vec3(0.12f),next+glm::vec3(0.12f))){drop.velocity={drop.velocity.x*0.45f,0,drop.velocity.z*0.45f};}else drop.position=next;}
    drops_.erase(std::remove_if(drops_.begin(),drops_.end(),[&](const Drop& d){if(d.age>0.25f&&glm::distance(d.position,player.position()+glm::vec3(0,0.8f,0))<1.5f){inventory.add(d.item,d.amount);return true;}return d.age>180.0f||d.position.y<-20.0f;}),drops_.end());
}

void SurvivalWorld::spawnDrop(const glm::vec3& position,Item item,int amount){
    if(item==Item::None)return;const std::uint32_t h=hashPosition(static_cast<int>(position.x*17),static_cast<int>(position.z*19),seed_+static_cast<std::uint32_t>(drops_.size()));
    const float vx=(static_cast<int>(h&255U)-127)/180.0f;const float vz=(static_cast<int>((h>>8U)&255U)-127)/180.0f;drops_.push_back({position,{vx,2.4f,vz},item,amount,0.0f});
}

std::vector<RenderCuboid> SurvivalWorld::renderCuboids()const{
    std::vector<RenderCuboid> parts;parts.reserve(animals_.size()*6+drops_.size());
    for(const Animal& a:animals_){glm::vec3 body,accent;switch(a.type){case AnimalType::Cow:body={0.38f,0.22f,0.12f};accent={0.72f,0.68f,0.56f};break;case AnimalType::Pig:body={0.94f,0.48f,0.57f};accent={0.72f,0.28f,0.34f};break;case AnimalType::Sheep:body={0.86f,0.86f,0.80f};accent={0.30f,0.28f,0.25f};break;default:body={0.28f,0.30f,0.34f};accent={0.52f,0.54f,0.58f};break;}
        glm::vec3 forward(a.heading.x,0,a.heading.y);parts.push_back({a.position+glm::vec3(0,0.75f,0),{1.15f,0.72f,0.62f},body});parts.push_back({a.position+forward*0.63f+glm::vec3(0,0.86f,0),{0.50f,0.52f,0.50f},accent});
        for(float sx:{-0.38f,0.38f})for(float sz:{-0.20f,0.20f})parts.push_back({a.position+glm::vec3(sx,0.28f,sz),{0.20f,0.56f,0.20f},accent*0.72f});}
    for(const Drop& d:drops_)parts.push_back({d.position,{0.25f,0.25f,0.25f},itemColor(d.item)});return parts;
}

