#include "Survival.h"
#include "SaveFile.h"
#include "Sound.h"
#include "UiLayout.h"
#include "Definitions.h"
#include "Player.h"
#include "World.h"
#include "SpriteManifest.h"
#include "Explosion.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <limits>
#include <random>
#include <sstream>
#include <thread>

namespace {
constexpr char InventoryMagic[8] = {'V', 'X', 'I', 'N', 'V', '4', '\0', '\0'};
constexpr char MobMagic[8] = {'V', 'X', 'M', 'O', 'B', '1', '\0', '\0'};
constexpr int PassiveMobCap = 26;
constexpr int HostileMobCap = 18;
constexpr int NaturalSpawnCandidates = 16;
constexpr int NightHostileBonusCandidates = 8;
constexpr float HostileCandidateChance = .52f;
constexpr std::size_t BillboardMobCount = sizeof(SpriteAssets) / sizeof(SpriteAssets[0]);
enum class BillboardBehavior { ExplosiveChase, MeleeChase, RangedAttack, WallClimbLunge };
struct BillboardMobDefinition {
    std::string name;
    BillboardBehavior behavior = BillboardBehavior::MeleeChase;
    float health = 20.0f;
    float chaseSpeed = 2.35f;
    float damage = 2.0f;
    float attackCooldown = 1.0f;
    float detectionRange = 16.0f;
    float spawnWeight = 1.0f;
    MobSoundType sound = MobSoundType::BillboardHostile;
};
const std::array<BillboardMobDefinition, BillboardMobCount>& billboardMobDefinitions() {
    static const auto definitions = [] {
        std::array<BillboardMobDefinition, BillboardMobCount> result{};
        for (std::size_t index = 0; index < BillboardMobCount; ++index) {
            const std::string filename = SpriteAssets[index].name;
            result[index].name = filename.substr(0, filename.find_last_of('.'));
            if (result[index].name == "HitoriGotoh") {
                result[index].behavior = BillboardBehavior::ExplosiveChase;
            } else if (result[index].name == "KitaIkuyo") {
                result[index].behavior = BillboardBehavior::MeleeChase;
                result[index].detectionRange = 35.0f;
            } else if (result[index].name == "NijikaIjichi") {
                result[index].behavior = BillboardBehavior::RangedAttack;
            } else if (result[index].name == "RyoYamada") {
                result[index].behavior = BillboardBehavior::WallClimbLunge;
            }
        }
        return result;
    }();
    return definitions;
}
MobSoundType mobSoundType(std::uint8_t id) {
    return id >= 6 && id < 6 + BillboardMobCount
        ? billboardMobDefinitions()[id - 6].sound : static_cast<MobSoundType>(id);
}
std::string billboardMobName(std::size_t index) {
    return billboardMobDefinitions()[index].name;
}
bool pick(Item i) {
    return itemDefinition(i).tool == ToolKind::Pickaxe;
}
bool axe(Item i) {
    return itemDefinition(i).tool == ToolKind::Axe;
}
bool shovel(Item i) {
    return itemDefinition(i).tool == ToolKind::Shovel;
}
int tier(Item i) {
    return itemDefinition(i).tier;
}
template <class T> bool wr(std::ofstream& f, const T& v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(v));
    return !!f;
}
template <class T> bool rd(std::ifstream& f, T& v) {
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return !!f;
}
bool same(Item item, const ItemStack& s) {
    return !s.empty() && s.item == item;
}
bool raySphere(const glm::vec3& o,
               const glm::vec3& direction,
               const glm::vec3& center,
               float reach,
               float radius,
               float& along) {
    glm::vec3 d = glm::normalize(direction), delta = center - o;
    along = glm::dot(delta, d);
    return along >= 0 && along <= reach && glm::length(delta - d * along) <= radius;
}
bool rayBox(const glm::vec3& origin, const glm::vec3& direction,
            const glm::vec3& minimum, const glm::vec3& maximum,
            float reach, float& along) {
    const glm::vec3 ray = glm::normalize(direction);
    float nearT = 0.0f, farT = reach;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(ray[axis]) < 0.00001f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return false;
            continue;
        }
        float first = (minimum[axis] - origin[axis]) / ray[axis];
        float second = (maximum[axis] - origin[axis]) / ray[axis];
        if (first > second) std::swap(first, second);
        nearT = std::max(nearT, first);
        farT = std::min(farT, second);
        if (farT < nearT) return false;
    }
    along = nearT;
    return true;
}
bool billboardTouchesSolid(const World& world, const glm::vec3& feet) {
    const glm::vec3 extent(BillboardMobHalfWidth - .01f, 0, BillboardMobHalfWidth - .01f);
    return world.aabbIntersectsSolid(feet - extent + glm::vec3(0, .02f, 0),
                                     feet + extent + glm::vec3(0, BillboardMobHeight - .01f, 0));
}

// Find a nearby collision surface that can support the mob's center and has
// clearance for its entire body. This is shared by navigation and step jumps.
bool billboardFooting(const World& world, const glm::vec2& center, float currentFeet,
                      float minimumRise, float maximumRise, float& surface) {
    const int x = static_cast<int>(std::floor(center.x));
    const int z = static_cast<int>(std::floor(center.y));
    bool found = false;
    float best = -std::numeric_limits<float>::infinity();
    for (int y = static_cast<int>(std::floor(currentFeet + minimumRise)) - 1;
         y <= static_cast<int>(std::floor(currentFeet + maximumRise)); ++y) {
        const Block block = world.getBlock(x, y, z);
        const BlockGeometryProperties shape = blockGeometry(block);
        if (!isSolid(block) || isWater(block) || block == Block::Cactus ||
            (shape.shape != BlockShape::PartialCube &&
             (shape.shape != BlockShape::Cube || blockDefinition(block).transparent))) continue;
        glm::vec3 low, high;
        if (!world.blockCollisionBounds({x, y, z}, low, high) ||
            center.x <= low.x || center.x >= high.x ||
            center.y <= low.z || center.y >= high.z) continue;
        if (high.y < currentFeet + minimumRise - .02f ||
            high.y > currentFeet + maximumRise + .02f || high.y <= best) continue;
        const glm::vec3 candidate(center.x, high.y + .01f, center.y);
        if (billboardTouchesSolid(world, candidate)) continue;
        best = high.y;
        found = true;
    }
    if (found) surface = best;
    return found;
}

using Recipe = RecipeInfo;
Recipe recipe(int w, int h, std::initializer_list<Ingredient> cells, Item output, int count,
              bool table, bool allowMirror = false, bool shapeless = false) {
    Recipe r;
    r.w = w;
    r.h = h;
    r.output = output;
    r.count = count;
    r.table = table;
    r.allowMirror = allowMirror;
    r.shapeless = shapeless;
    r.category = isTool(output) || isSword(output) ? RecipeCategory::Tools
                 : isFood(output) ? RecipeCategory::Food
                 : output == Item::Torch ? RecipeCategory::Misc
                 : itemToBlock(output) != Block::Air ? RecipeCategory::Building
                 : RecipeCategory::Misc;
    std::copy(cells.begin(), cells.end(), r.cells.begin());
    return r;
}
const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> all = []() {
        std::vector<Recipe> r;
        r.push_back(recipe(1, 1, {Item::Log}, Item::Planks, 4, false));
        r.push_back(recipe(1, 2, {IngredientGroup::Planks, IngredientGroup::Planks}, Item::Stick, 4, false));
        r.push_back(recipe(1, 2, {Item::Coal, Item::Stick}, Item::Torch, 4, false));
        r.push_back(recipe(2,
                           2,
                           {IngredientGroup::Planks, IngredientGroup::Planks,
                            IngredientGroup::Planks, IngredientGroup::Planks},
                           Item::CraftingTable,
                           1,
                           false));
        r.push_back(recipe(3, 1, {Item::Wheat, Item::Wheat, Item::Wheat}, Item::Bread, 1, true));
        for (Item m : {Item::Planks, Item::Cobblestone, Item::IronIngot, Item::GoldIngot, Item::Diamond}) {
            const Ingredient material = m == Item::Planks ? IngredientGroup::Planks : Ingredient(m);
            Item p = m == Item::Planks
                         ? Item::WoodPickaxe
                         : (m == Item::Cobblestone
                                ? Item::StonePickaxe
                                : (m == Item::IronIngot
                                       ? Item::IronPickaxe
                                       : (m == Item::GoldIngot ? Item::GoldPickaxe
                                                               : Item::DiamondPickaxe)));
            Item a = m == Item::Planks ? Item::WoodAxe
                                       : (m == Item::Cobblestone
                                              ? Item::StoneAxe
                                              : (m == Item::IronIngot
                                                     ? Item::IronAxe
                                                     : (m == Item::GoldIngot ? Item::GoldAxe
                                                                             : Item::DiamondAxe)));
            Item s = m == Item::Planks ? Item::WoodShovel
                         : (m == Item::Cobblestone
                                              ? Item::StoneShovel
                                              : (m == Item::IronIngot
                                                     ? Item::IronShovel
                                                     : (m == Item::GoldIngot ? Item::GoldShovel
                                                                             : Item::DiamondShovel)));
            r.push_back(recipe(
                3,
                3,
                {material, material, material, Item::None, Item::Stick, Item::None,
                 Item::None, Item::Stick, Item::None},
                p,
                1,
                true));
            r.push_back(recipe(2, 3,
                {material, material, material, Item::Stick, Item::None, Item::Stick},
                a, 1, true, true));
            r.push_back(recipe(1, 3, {material, Item::Stick, Item::Stick}, s, 1, true));
            Item sword = m == Item::Planks
                             ? Item::WoodSword
                             : (m == Item::Cobblestone
                                    ? Item::StoneSword
                                    : (m == Item::IronIngot
                                           ? Item::IronSword
                                           : (m == Item::GoldIngot ? Item::GoldSword
                                                                   : Item::DiamondSword)));
            r.push_back(recipe(1, 3, {material, material, Item::Stick}, sword, 1, true));
        }
        r.push_back(recipe(1, 1, {Item::BirchLog}, Item::BirchPlanks, 4, false));
        r.push_back(recipe(2,
                           2,
                           {Item::Stone, Item::Stone, Item::Stone, Item::Stone},
                           Item::StoneBricks,
                           4,
                           false));
        r.push_back(recipe(2, 2, {Item::Snow, Item::Snow, Item::Snow, Item::Snow}, Item::SnowBlock, 1, false));
        r.push_back(recipe(2, 1, {Item::Diorite, Item::Cobblestone},
                           Item::Andesite, 2, false, false, true));
        r.push_back(recipe(3,
                           3,
                           {Item::Cobblestone,
                            Item::Cobblestone,
                            Item::Cobblestone,
                            Item::Cobblestone,
                            Item::None,
                            Item::Cobblestone,
                            Item::Cobblestone,
                            Item::Cobblestone,
                            Item::Cobblestone},
                           Item::Furnace,
                           1,
                           true));
        r.push_back(recipe(3,
                           3,
                           {IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            Item::None,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks},
                           Item::Chest,
                           1,
                           true));
        r.push_back(recipe(3,
                           3,
                           {Item::Stick,
                            Item::None,
                            Item::Stick,
                            Item::Stick,
                            Item::Stick,
                            Item::Stick,
                            Item::Stick,
                            Item::None,
                            Item::Stick},
                           Item::Ladder,
                           3,
                           true));
        r.push_back(recipe(2,
                           2,
                           {Item::Cobblestone,
                            Item::TallGrass,
                            Item::TallGrass,
                            Item::Cobblestone},
                           Item::MossyCobblestone,
                           2,
                           false, false, true));
        r.push_back(recipe(2,
                           2,
                           {Item::StoneBricks,
                            Item::TallGrass,
                            Item::TallGrass,
                            Item::StoneBricks},
                           Item::MossyStoneBricks,
                           2,
                           false, false, true));
        r.push_back(recipe(2,
                           3,
                           {IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks,
                            IngredientGroup::Planks},
                           Item::WoodenDoor,
                           3,
                           true));
        r.push_back(recipe(3,
                           1,
                           {IngredientGroup::Planks, IngredientGroup::Planks,
                            IngredientGroup::Planks},
                           Item::WoodenSlab,
                           6,
                           true));
        r.push_back(recipe(3,
                           1,
                           {Item::Stone, Item::Stone, Item::Stone},
                           Item::StoneSlab,
                           6,
                           true));
        return r;
    }();
    return all;
}
} // namespace

const std::vector<RecipeInfo>& craftingRecipes() {
    return recipes();
}

Inventory::Inventory() {
    add(Item::Grass, 12);
    add(Item::Log, 4);
    add(Item::Torch, 6);
    add(Item::WoodPickaxe, 1);
    add(Item::Seeds, 6);
    add(Item::Bread, 2);
}
void Inventory::selectSlot(int s) {
    selectedSlot_ = std::clamp(s, 0, 8);
}
void Inventory::cycleSlot(int d) {
    selectedSlot_ = (selectedSlot_ + d) % 9;
    if (selectedSlot_ < 0)
        selectedSlot_ += 9;
}
bool Inventory::pickBlock(Item item, bool creativeMode) {
    if (item == Item::None)
        return false;
    for (int slot = 0; slot < HotbarSlots; ++slot) {
        if (slots_[static_cast<std::size_t>(slot)].item == item &&
            !slots_[static_cast<std::size_t>(slot)].empty()) {
            selectedSlot_ = slot;
            return true;
        }
    }
    if (!creativeMode)
        return false;
    int destination = selectedSlot_;
    if (!slots_[static_cast<std::size_t>(destination)].empty()) {
        for (int slot = 0; slot < HotbarSlots; ++slot) {
            if (slots_[static_cast<std::size_t>(slot)].empty()) {
                destination = slot;
                break;
            }
        }
    }
    slots_[static_cast<std::size_t>(destination)] =
        {item, isTool(item) ? 1 : maxStack(item), maxDurability(item)};
    selectedSlot_ = destination;
    return true;
}
Item Inventory::selectedItem() const {
    return selectedStack().empty() ? Item::None : selectedStack().item;
}
const ItemStack& Inventory::selectedStack() const {
    return slots_[selectedSlot_];
}
const ItemStack& Inventory::slot(int i) const {
    static ItemStack none;
    return i >= 0 && i < TotalSlots ? slots_[i] : none;
}
const ItemStack& Inventory::craftSlot(int i) const {
    static ItemStack none;
    return i >= 0 && i < CraftSlots ? crafting_[i] : none;
}
int Inventory::count(Item item) const {
    int n = 0;
    for (const auto& s : slots_)
        if (s.item == item)
            n += s.count;
    return n;
}
int Inventory::maxStack(Item i) {
    return itemDefinition(i).maxStack;
}
int Inventory::maxDurability(Item i) {
    return itemDefinition(i).durability;
}
void Inventory::normalize(ItemStack& s) {
    if (s.item == Item::None || s.count <= 0) {
        s.clear();
        return;
    }
    s.count = std::min(s.count, maxStack(s.item));
    int d = maxDurability(s.item);
    s.durability = d ? std::clamp(s.durability, 1, d) : 0;
}
bool Inventory::canMerge(const ItemStack& a, const ItemStack& b) {
    return !a.empty() && !b.empty() && a.item == b.item && !isTool(a.item);
}
int Inventory::add(Item item, int amount, int durability) {
    if (item == Item::None || amount <= 0)
        return amount;
    int max = maxStack(item);
    if (!isTool(item))
        for (auto& s : slots_)
            if (s.item == item && s.count < max) {
                int n = std::min(amount, max - s.count);
                s.count += n;
                amount -= n;
                if (!amount)
                    return 0;
            }
    for (auto& s : slots_)
        if (s.empty()) {
            int n = std::min(amount, max);
            s = {item, n, isTool(item) ? (durability < 0 ? maxDurability(item) : durability) : 0};
            normalize(s);
            amount -= n;
            if (!amount)
                return 0;
        }
    return amount;
}
bool Inventory::remove(Item item, int amount) {
    if (count(item) < amount)
        return false;
    for (auto& s : slots_)
        if (s.item == item && amount) {
            int n = std::min(amount, s.count);
            s.count -= n;
            amount -= n;
            normalize(s);
        }
    return true;
}
bool Inventory::consumeSelected(int n) {
    auto& s = slots_[selectedSlot_];
    if (s.count < n)
        return false;
    s.count -= n;
    normalize(s);
    return true;
}
bool Inventory::damageSelectedTool(int n) {
    auto& s = slots_[selectedSlot_];
    if (!isTool(s.item))
        return false;
    s.durability -= n;
    if (s.durability <= 0)
        s.clear();
    return true;
}

void Inventory::clear() {
    for (ItemStack& stack : slots_)
        stack.clear();
    for (ItemStack& stack : crafting_)
        stack.clear();
    cursor_.clear();
    selectedSlot_ = 0;
}

void Inventory::setCreativeCursor(Item item) {
    if (item == Item::None) {
        cursor_.clear();
        return;
    }
    cursor_ = {item,
               isTool(item) ? 1 : maxStack(item),
               isTool(item) ? maxDurability(item) : 0};
}

bool Inventory::handleCreativeClick(
    double mouseX, double mouseY, int width, int height, bool rightClick) {
    const UiHit hit = UiLayout::hit(
        UiMode::Creative, mouseX, mouseY, width, height, creativeCatalog().size());
    if (hit.kind == UiSlotKind::CreativeItem) {
        setCreativeCursor(creativeCatalog()[static_cast<std::size_t>(hit.index)]);
        if (rightClick && !cursor_.empty() && !isTool(cursor_.item))
            cursor_.count = 1;
        return true;
    }
    if (hit.kind == UiSlotKind::PlayerInventorySlot) {
        clickStack(slots_[static_cast<std::size_t>(hit.index)], cursor_, rightClick);
        return true;
    }
    return false;
}
void Inventory::clickStack(ItemStack& slot, ItemStack& cursor, bool right) {
    normalize(slot);
    normalize(cursor);
    if (!right) {
        if (cursor.empty()) {
            cursor = slot;
            slot.clear();
        } else if (slot.empty()) {
            slot = cursor;
            cursor.clear();
        } else if (canMerge(slot, cursor)) {
            int n = std::min(cursor.count, maxStack(slot.item) - slot.count);
            slot.count += n;
            cursor.count -= n;
            normalize(cursor);
        } else
            std::swap(slot, cursor);
    } else {
        if (cursor.empty() && !slot.empty()) {
            int n = (slot.count + 1) / 2;
            cursor = slot;
            cursor.count = n;
            slot.count -= n;
            normalize(slot);
        } else if (!cursor.empty() && slot.empty()) {
            slot = cursor;
            slot.count = 1;
            --cursor.count;
            normalize(cursor);
        } else if (canMerge(slot, cursor) && slot.count < maxStack(slot.item)) {
            ++slot.count;
            --cursor.count;
            normalize(cursor);
        }
    }
}

bool Inventory::findCraftingMatch(bool table, ItemStack& out, std::vector<int>* used) const {
    const int grid = table ? 3 : 2;
    for (const Recipe& r : recipes()) {
        if ((r.table && !table) || r.w > grid || r.h > grid)
            continue;
        if (r.shapeless) {
            std::vector<int> slots;
            for (int y = 0; y < grid; ++y)
                for (int x = 0; x < grid; ++x)
                    if (!crafting_[static_cast<std::size_t>(y * 3 + x)].empty())
                        slots.push_back(y * 3 + x);
            int required = 0;
            for (int i = 0; i < r.w * r.h; ++i)
                required += !r.cells[static_cast<std::size_t>(i)].empty();
            if (static_cast<int>(slots.size()) != required)
                continue;
            std::function<bool(int, int)> assign = [&](int cell, int claimed) {
                if (cell == r.w * r.h)
                    return true;
                const Ingredient& ingredient = r.cells[static_cast<std::size_t>(cell)];
                if (ingredient.empty())
                    return assign(cell + 1, claimed);
                for (int i = 0; i < required; ++i)
                    if (!(claimed & (1 << i)) &&
                        ingredient.accepts(crafting_[static_cast<std::size_t>(slots[i])].item) &&
                        assign(cell + 1, claimed | (1 << i)))
                        return true;
                return false;
            };
            if (!assign(0, 0))
                continue;
            out = {r.output, r.count, maxDurability(r.output)};
            if (used) *used = slots;
            return true;
        }
        for (int oy = 0; oy <= grid - r.h; ++oy)
            for (int ox = 0; ox <= grid - r.w; ++ox)
                for (int mirror = 0; mirror < (r.allowMirror && r.w > 1 ? 2 : 1); ++mirror) {
                    bool ok = true;
                    std::vector<int> indices;
                    for (int y = 0; y < grid; ++y)
                        for (int x = 0; x < grid; ++x) {
                            Ingredient expected;
                            if (x >= ox && x < ox + r.w && y >= oy && y < oy + r.h) {
                                int rx = x - ox;
                                if (mirror)
                                    rx = r.w - 1 - rx;
                                expected = r.cells[(y - oy) * r.w + rx];
                            }
                            int ci = y * 3 + x;
                            const Item actual = crafting_[ci].empty() ? Item::None : crafting_[ci].item;
                            if (!expected.accepts(actual)) {
                                ok = false;
                                break;
                            }
                            if (!expected.empty())
                                indices.push_back(ci);
                        }
                    if (ok) {
                        out = {r.output, r.count, maxDurability(r.output)};
                        if (used)
                            *used = indices;
                        return true;
                    }
                }
    }
    out.clear();
    return false;
}
ItemStack Inventory::craftingOutput(bool table) const {
    ItemStack out;
    findCraftingMatch(table, out, nullptr);
    return out;
}
bool Inventory::takeCraftingOutput(bool table) {
    ItemStack out;
    std::vector<int> used;
    if (!findCraftingMatch(table, out, &used))
        return false;
    if (cursor_.empty())
        cursor_ = out;
    else if (canMerge(cursor_, out) && cursor_.count + out.count <= maxStack(out.item))
        cursor_.count += out.count;
    else
        return false;
    for (int i : used) {
        --crafting_[i].count;
        normalize(crafting_[i]);
    }
    return true;
}

int Inventory::craftOutputToInventory(bool table) {
    int crafted = 0;
    for (int attempt = 0; attempt < 64; ++attempt) {
        ItemStack output;
        std::vector<int> used;
        if (!findCraftingMatch(table, output, &used))
            break;
        int capacity = 0;
        for (const ItemStack& stack : slots_) {
            if (stack.empty())
                capacity += maxStack(output.item);
            else if (canMerge(stack, output))
                capacity += maxStack(output.item) - stack.count;
        }
        if (capacity < output.count)
            break;
        for (int index : used) {
            --crafting_[static_cast<std::size_t>(index)].count;
            normalize(crafting_[static_cast<std::size_t>(index)]);
        }
        add(output.item, output.count, output.durability);
        ++crafted;
    }
    return crafted;
}

bool Inventory::recipeCraftable(int recipeIndex, bool table) const {
    Inventory candidate = *this;
    return candidate.fillRecipe(recipeIndex, table, false);
}

bool Inventory::fillRecipe(int recipeIndex, bool table, bool maximize) {
    const auto& all = craftingRecipes();
    if (recipeIndex < 0 || recipeIndex >= static_cast<int>(all.size()))
        return false;
    const RecipeInfo& recipe = all[static_cast<std::size_t>(recipeIndex)];
    if ((recipe.table && !table) || recipe.w > (table ? 3 : 2) ||
        recipe.h > (table ? 3 : 2))
        return false;
    const int limit = maximize ? 64 : 1;
    const int grid = table ? 3 : 2;
    auto prepare = [&](Inventory& candidate, int craftCount) {
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x) {
                const Ingredient ingredient = x < recipe.w && y < recipe.h
                    ? recipe.cells[static_cast<std::size_t>(y * recipe.w + x)] : Ingredient{};
                ItemStack& destination = candidate.crafting_[static_cast<std::size_t>(y * 3 + x)];
                if (x >= grid || y >= grid || ingredient.empty()) {
                    if (!destination.empty()) return false;
                    continue;
                }
                if (!destination.empty()) {
                    if (!ingredient.accepts(destination.item)) return false;
                    const int needed = std::max(0, craftCount - destination.count);
                    if (destination.count + needed > maxStack(destination.item) ||
                        !candidate.remove(destination.item, needed)) return false;
                    destination.count += needed;
                    continue;
                }
                Item chosen = Item::None;
                int available = 0;
                for (int itemIndex = static_cast<int>(Item::Grass);
                     itemIndex < static_cast<int>(Item::Count); ++itemIndex) {
                    const Item item = static_cast<Item>(itemIndex);
                    const int stock = candidate.count(item);
                    if (ingredient.accepts(item) && stock >= craftCount &&
                        stock > available && maxStack(item) >= craftCount) {
                        chosen = item;
                        available = stock;
                    }
                }
                if (chosen == Item::None || !candidate.remove(chosen, craftCount))
                    return false;
                destination = {chosen, craftCount, 0};
            }
        return candidate.craftingOutput(table).item == recipe.output;
    };

    Inventory best = *this;
    int bestCount = 0;
    if (!recipe.shapeless)
        for (int craftCount = limit; craftCount >= 1; --craftCount) {
            Inventory candidate = *this;
            if (prepare(candidate, craftCount)) {
                best = std::move(candidate);
                bestCount = craftCount;
                if (!maximize) { *this = std::move(best); return true; }
                break;
            }
        }

    Inventory base = *this;
    base.returnCraftingItems();
    bool returnedAll = true;
    for (const ItemStack& stack : base.crafting_)
        returnedAll &= stack.empty();
    if (returnedAll)
        for (int craftCount = limit; craftCount > bestCount; --craftCount) {
            Inventory candidate = base;
            if (prepare(candidate, craftCount)) {
                best = std::move(candidate);
                bestCount = craftCount;
                break;
            }
        }
    if (bestCount == 0) return false;
    *this = std::move(best);
    return true;
}

bool Inventory::shiftIngredientToCrafting(int playerSlot, int recipeIndex, bool table) {
    if (playerSlot < 0 || playerSlot >= TotalSlots)
        return false;
    const auto& all = craftingRecipes();
    if (recipeIndex < 0 || recipeIndex >= static_cast<int>(all.size()))
        return false;
    const RecipeInfo& recipe = all[static_cast<std::size_t>(recipeIndex)];
    if ((recipe.table && !table) || recipe.w > (table ? 3 : 2) ||
        recipe.h > (table ? 3 : 2))
        return false;
    ItemStack& source = slots_[static_cast<std::size_t>(playerSlot)];
    if (source.empty())
        return false;
    bool movedAny = false;
    // Seed every required cell before adding surplus to existing stacks. This makes
    // Shift-click useful for recipes with repeated ingredients (for example, tools).
    for (int y = 0; y < recipe.h; ++y) {
        for (int x = 0; x < recipe.w; ++x) {
            if (!recipe.cells[static_cast<std::size_t>(y * recipe.w + x)].accepts(source.item))
                continue;
            ItemStack& destination = crafting_[static_cast<std::size_t>(y * 3 + x)];
            if (!destination.empty())
                continue;
            destination = {source.item, 1, source.durability};
            --source.count;
            movedAny = true;
            normalize(source);
            if (source.empty())
                return true;
        }
    }
    for (int y = 0; y < recipe.h; ++y) {
        for (int x = 0; x < recipe.w; ++x) {
            if (!recipe.cells[static_cast<std::size_t>(y * recipe.w + x)].accepts(source.item))
                continue;
            ItemStack& destination = crafting_[static_cast<std::size_t>(y * 3 + x)];
            if (destination.item != source.item)
                continue;
            const int moved = std::min(source.count, maxStack(source.item) - destination.count);
            if (moved <= 0)
                continue;
            destination.count += moved;
            source.count -= moved;
            movedAny = true;
            normalize(source);
            if (source.empty())
                return true;
        }
    }
    return movedAny;
}

bool Inventory::gatherMatchingToCursor() {
    if (cursor_.empty() || isTool(cursor_.item))
        return false;
    const int capacity = maxStack(cursor_.item) - cursor_.count;
    if (capacity <= 0)
        return false;
    bool movedAny = false;
    auto gather = [&](ItemStack& stack) {
        if (!canMerge(stack, cursor_))
            return;
        const int moved = std::min(stack.count, maxStack(cursor_.item) - cursor_.count);
        cursor_.count += moved;
        stack.count -= moved;
        normalize(stack);
        movedAny = movedAny || moved > 0;
    };
    for (ItemStack& stack : slots_)
        gather(stack);
    for (ItemStack& stack : crafting_)
        gather(stack);
    return movedAny;
}
bool Inventory::handleInventoryClick(
    double mx, double my, int w, int h, bool right, bool table, bool shift) {
    const UiHit hit = UiLayout::hit(
        table ? UiMode::CraftingTable : UiMode::Inventory, mx, my, w, h);
    auto quick = [&](int source, int first, int last) {
        ItemStack moving = slots_[source];
        if (moving.empty())
            return true;
        if (!isTool(moving.item))
            for (int i = first; i < last && moving.count; ++i)
                if (canMerge(slots_[i], moving)) {
                    int n = std::min(moving.count, maxStack(moving.item) - slots_[i].count);
                    slots_[i].count += n;
                    moving.count -= n;
                }
        for (int i = first; i < last && moving.count; ++i)
            if (slots_[i].empty()) {
                int n = std::min(moving.count, maxStack(moving.item));
                slots_[i] = moving;
                slots_[i].count = n;
                moving.count -= n;
            }
        slots_[source] = moving;
        normalize(slots_[source]);
        return true;
    };
    if (hit.kind == UiSlotKind::CraftingOutput) {
        if (shift)
            craftOutputToInventory(table);
        else
            takeCraftingOutput(table);
        return true; // An empty/full output slot is still a UI click, not a world drop.
    }
    if (hit.kind == UiSlotKind::CraftingSlot) {
        if (shift) {
            ItemStack& stack = crafting_[static_cast<std::size_t>(hit.index)];
            moveExternalToInventory(stack);
            return true;
        }
        clickStack(crafting_[static_cast<std::size_t>(hit.index)], cursor_, right);
        return true;
    }
    if (hit.kind == UiSlotKind::PlayerInventorySlot) {
        if (shift)
            return hit.index >= HotbarSlots
                       ? quick(hit.index, 0, HotbarSlots)
                       : quick(hit.index, HotbarSlots, TotalSlots);
        clickStack(slots_[static_cast<std::size_t>(hit.index)], cursor_, right);
        return true;
    }
    return false;
}

bool Inventory::clickPlayerSlot(int index, bool rightClick) {
    if (index < 0 || index >= TotalSlots)
        return false;
    clickStack(slots_[static_cast<std::size_t>(index)], cursor_, rightClick);
    return true;
}

bool Inventory::clickExternalSlot(ItemStack& stack, bool rightClick, bool outputOnly) {
    normalize(stack);
    if (outputOnly) {
        if (stack.empty())
            return false;
        if (cursor_.empty()) {
            cursor_ = stack;
            stack.clear();
            return true;
        }
        if (canMerge(cursor_, stack) && cursor_.count + stack.count <= maxStack(stack.item)) {
            cursor_.count += stack.count;
            stack.clear();
            return true;
        }
        return false;
    }
    clickStack(stack, cursor_, rightClick);
    return true;
}

bool Inventory::moveExternalToInventory(ItemStack& stack) {
    if (stack.empty())
        return false;
    const int before = stack.count;
    stack.count = add(stack.item, stack.count, stack.durability);
    normalize(stack);
    return stack.count != before;
}

bool Inventory::movePlayerToExternal(int index, ItemStack* stacks, int stackCount) {
    if (index < 0 || index >= TotalSlots || !stacks || stackCount <= 0)
        return false;
    ItemStack& source = slots_[static_cast<std::size_t>(index)];
    if (source.empty())
        return false;
    const int before = source.count;
    if (!isTool(source.item)) {
        for (int i = 0; i < stackCount && source.count > 0; ++i) {
            if (!canMerge(stacks[i], source))
                continue;
            const int moved = std::min(source.count, maxStack(source.item) - stacks[i].count);
            stacks[i].count += moved;
            source.count -= moved;
        }
    }
    for (int i = 0; i < stackCount && source.count > 0; ++i) {
        if (!stacks[i].empty())
            continue;
        const int moved = std::min(source.count, maxStack(source.item));
        stacks[i] = source;
        stacks[i].count = moved;
        source.count -= moved;
    }
    normalize(source);
    return source.count != before;
}
ItemStack Inventory::takeCursor(bool one) {
    ItemStack r;
    if (cursor_.empty())
        return r;
    if (one) {
        r = cursor_;
        r.count = 1;
        --cursor_.count;
        normalize(cursor_);
    } else {
        r = cursor_;
        cursor_.clear();
    }
    return r;
}
ItemStack Inventory::takeSelected(bool entireStack) {
    ItemStack& selected = slots_[static_cast<std::size_t>(selectedSlot_)];
    if (selected.empty())
        return {};
    ItemStack dropped = selected;
    if (entireStack) {
        selected.clear();
    } else {
        dropped.count = 1;
        --selected.count;
        normalize(selected);
    }
    return dropped;
}
void Inventory::returnCraftingItems() {
    for (auto& s : crafting_)
        if (!s.empty()) {
            int left = add(s.item, s.count, s.durability);
            s.count = left;
            normalize(s);
        }
}
bool Inventory::save(const std::string& p, std::uint32_t seed) const {
    return SaveFile::write(p, std::ios::binary, [&](std::ofstream& f) {
    f.write(InventoryMagic, 8);
    wr(f, seed);
    wr(f, selectedSlot_);
    auto ws = [&](const ItemStack& s) {
        std::uint8_t i = itemDefinition(s.item).saveId;
        std::int16_t c = static_cast<std::int16_t>(s.count),
                     d = static_cast<std::int16_t>(s.durability);
        return wr(f, i) && wr(f, c) && wr(f, d);
    };
    for (const auto& s : slots_)
        if (!ws(s))
            return false;
    for (const auto& s : crafting_)
        if (!ws(s))
            return false;
    return ws(cursor_);
    });
}
bool Inventory::load(const std::string& p, std::uint32_t seed) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return false;
    char m[8]{};
    std::uint32_t stored = 0;
    f.read(m, 8);
    if (std::memcmp(m, InventoryMagic, 8) || !rd(f, stored) || stored != seed ||
        !rd(f, selectedSlot_))
        return false;
    auto rs = [&](ItemStack& s) {
        std::uint8_t i;
        std::int16_t c, d;
        Item loadedItem = Item::None;
        if (!rd(f, i) || !rd(f, c) || !rd(f, d) || !itemFromSaveId(i, loadedItem))
            return false;
        s = {loadedItem, c, d};
        normalize(s);
        return true;
    };
    for (auto& s : slots_)
        if (!rs(s))
            return false;
    for (auto& s : crafting_)
        if (!rs(s))
            return false;
    if (!rs(cursor_))
        return false;
    selectSlot(selectedSlot_);
    return true;
}
bool Inventory::runCraftingSelfTest(std::string& report) {
    Inventory inventory;
    inventory.clear();
    auto test = [&](bool table,
                    std::initializer_list<std::pair<int, Item>> ingredients,
                    Item expected,
                    int expectedCount = 1) {
        for (ItemStack& stack : inventory.crafting_)
            stack.clear();
        inventory.cursor_.clear();
        for (const auto& ingredient : ingredients)
            inventory.crafting_[ingredient.first] = {ingredient.second, 1, 0};
        const ItemStack output = inventory.craftingOutput(table);
        if (output.item != expected || output.count != expectedCount ||
            !inventory.takeCraftingOutput(table))
            return false;
        for (const auto& ingredient : ingredients)
            if (!inventory.crafting_[ingredient.first].empty())
                return false;
        return true;
    };

    const bool ok =
        test(false, {{0, Item::Log}}, Item::Planks, 4) &&
        test(false, {{0, Item::BirchLog}}, Item::BirchPlanks, 4) &&
        test(false,
             {{0, Item::Planks}, {1, Item::Planks}, {3, Item::Planks}, {4, Item::Planks}},
             Item::CraftingTable) &&
        test(false,
             {{0, Item::StoneBricks},
              {1, Item::TallGrass},
              {3, Item::TallGrass},
              {4, Item::StoneBricks}},
             Item::MossyStoneBricks,
             2) &&
        test(true,
             {{0, Item::Planks},
              {1, Item::Planks},
              {2, Item::Planks},
              {4, Item::Stick},
              {7, Item::Stick}},
             Item::WoodPickaxe) &&
        test(true,
             {{0, Item::IronIngot},
              {1, Item::IronIngot},
              {2, Item::IronIngot},
              {4, Item::Stick},
              {7, Item::Stick}},
             Item::IronPickaxe) &&
        test(true,
             {{0, Item::Cobblestone},
              {1, Item::Cobblestone},
              {2, Item::Cobblestone},
              {3, Item::Cobblestone},
              {5, Item::Cobblestone},
              {6, Item::Cobblestone},
              {7, Item::Cobblestone},
              {8, Item::Cobblestone}},
             Item::Furnace) &&
        test(true,
             {{0, Item::Planks},
              {1, Item::Planks},
              {2, Item::Planks},
              {3, Item::Planks},
              {5, Item::Planks},
              {6, Item::Planks},
              {7, Item::Planks},
              {8, Item::Planks}},
             Item::Chest) &&
        test(true,
             {{0, Item::Diamond},
              {1, Item::Diamond},
              {2, Item::Diamond},
              {4, Item::Stick},
              {7, Item::Stick}},
             Item::DiamondPickaxe) &&
        test(true,
             {{1, Item::GoldIngot}, {4, Item::GoldIngot}, {7, Item::Stick}},
             Item::GoldSword) &&
        test(true,
             {{0, Item::Stick},
              {2, Item::Stick},
              {3, Item::Stick},
              {4, Item::Stick},
              {5, Item::Stick},
              {6, Item::Stick},
              {8, Item::Stick}},
             Item::Ladder,
             3) &&
        test(true,
             {{0, Item::Planks},
              {1, Item::Planks},
              {3, Item::Planks},
              {4, Item::Planks},
              {6, Item::Planks},
              {7, Item::Planks}},
             Item::WoodenDoor,
             3) &&
        test(true, {{3, Item::Wheat}, {4, Item::Wheat}, {5, Item::Wheat}}, Item::Bread);
    int plankRecipe = -1;
    int pickaxeRecipe = -1;
    for (int index = 0; index < static_cast<int>(craftingRecipes().size()); ++index) {
        const Item output = craftingRecipes()[static_cast<std::size_t>(index)].output;
        if (output == Item::Planks) plankRecipe = index;
        if (output == Item::WoodPickaxe) pickaxeRecipe = index;
    }
    Inventory fastCraft;
    fastCraft.clear();
    fastCraft.add(Item::Log, 3);
    bool improved = plankRecipe >= 0 && pickaxeRecipe >= 0 &&
                    fastCraft.recipeCraftable(plankRecipe, false) &&
                    !fastCraft.recipeCraftable(pickaxeRecipe, false) &&
                    fastCraft.fillRecipe(plankRecipe, false, true) &&
                    fastCraft.craftSlot(0).count == 3 &&
                    fastCraft.craftOutputToInventory(false) == 3 &&
                    fastCraft.count(Item::Planks) == 12 &&
                    fastCraft.craftSlot(0).empty();
    fastCraft.clear();
    improved = improved && !fastCraft.fillRecipe(plankRecipe, false, false);
    for (ItemStack& stack : fastCraft.slots_)
        stack = {Item::Stone, 64, 0};
    fastCraft.crafting_[0] = {Item::Log, 2, 0};
    improved = improved && fastCraft.craftOutputToInventory(false) == 0 &&
               fastCraft.crafting_[0].count == 2;
    fastCraft.clear();
    fastCraft.slots_[0] = {Item::Planks, 3, 0};
    fastCraft.slots_[1] = {Item::Stick, 2, 0};
    improved = improved && fastCraft.shiftIngredientToCrafting(0, pickaxeRecipe, true) &&
               fastCraft.shiftIngredientToCrafting(1, pickaxeRecipe, true) &&
               fastCraft.crafting_[0].count == 1 &&
               fastCraft.crafting_[1].count == 1 &&
               fastCraft.crafting_[2].count == 1 &&
               fastCraft.crafting_[4].count == 1 &&
               fastCraft.crafting_[7].count == 1 &&
               fastCraft.craftingOutput(true).item == Item::WoodPickaxe;
    fastCraft.clear();
    fastCraft.cursor_ = {Item::Diamond, 2, 0};
    const UiRect emptyOutput = UiLayout::craftingOutput(1280, 720);
    improved = improved && fastCraft.handleInventoryClick(
                    emptyOutput.x + 22, emptyOutput.y + 22, 1280, 720,
                    false, false, true) &&
               fastCraft.cursor_.item == Item::Diamond && fastCraft.cursor_.count == 2;
    const std::array<Item, 5> materials = {Item::Planks, Item::Cobblestone,
        Item::IronIngot, Item::GoldIngot, Item::Diamond};
    const std::array<Item, 5> picks = {Item::WoodPickaxe, Item::StonePickaxe,
        Item::IronPickaxe, Item::GoldPickaxe, Item::DiamondPickaxe};
    const std::array<Item, 5> axes = {Item::WoodAxe, Item::StoneAxe,
        Item::IronAxe, Item::GoldAxe, Item::DiamondAxe};
    const std::array<Item, 5> shovels = {Item::WoodShovel, Item::StoneShovel,
        Item::IronShovel, Item::GoldShovel, Item::DiamondShovel};
    const std::array<Item, 5> swords = {Item::WoodSword, Item::StoneSword,
        Item::IronSword, Item::GoldSword, Item::DiamondSword};
    for (std::size_t i = 0; i < materials.size(); ++i) {
        const Item m = materials[i];
        improved &= test(true, {{0,m},{1,m},{2,m},{4,Item::Stick},{7,Item::Stick}}, picks[i]);
        improved &= test(true, {{0,m},{1,m},{3,m},{4,Item::Stick},{7,Item::Stick}}, axes[i]);
        improved &= test(true, {{1,m},{2,m},{4,Item::Stick},{5,m},{7,Item::Stick}}, axes[i]);
        improved &= test(true, {{1,m},{4,Item::Stick},{7,Item::Stick}}, shovels[i]);
        improved &= test(true, {{1,m},{4,m},{7,Item::Stick}}, swords[i]);
    }
    improved &= test(false, {{0,Item::Coal},{3,Item::Stick}}, Item::Torch, 4);
    improved &= test(true, {{4,Item::BirchPlanks},{5,Item::Planks},
        {7,Item::Planks},{8,Item::BirchPlanks}}, Item::CraftingTable);
    improved &= test(true, {{3,Item::Stone},{4,Item::Stone},{5,Item::Stone}},
        Item::StoneSlab, 6);
    improved &= test(true, {{3,Item::BirchPlanks},{4,Item::Planks},
        {5,Item::BirchPlanks}}, Item::WoodenSlab, 6);
    improved &= test(false, {{0,Item::TallGrass},{1,Item::Cobblestone},
        {3,Item::Cobblestone},{4,Item::TallGrass}}, Item::MossyCobblestone, 2);
    improved &= test(false, {{0,Item::Cobblestone},{4,Item::Diorite}},
        Item::Andesite, 2);
    fastCraft.clear();
    for (int slot : {0,1,2,3,5,6,7,8})
        fastCraft.crafting_[static_cast<std::size_t>(slot)] = {Item::Cobblestone, 1, 0};
    improved &= fastCraft.craftingOutput(true).item == Item::Furnace &&
                fastCraft.craftingOutput(false).empty();
    fastCraft.crafting_[4] = {Item::Cobblestone, 1, 0};
    improved &= fastCraft.craftingOutput(true).empty();
    fastCraft.crafting_[4].clear();
    fastCraft.crafting_[0] = {Item::Stone, 1, 0};
    improved &= fastCraft.craftingOutput(true).empty();
    fastCraft.crafting_[0].clear();
    improved &= fastCraft.craftingOutput(true).empty();
    fastCraft.clear();
    fastCraft.add(Item::Planks, 16);
    fastCraft.add(Item::BirchPlanks, 16);
    int chestRecipe = -1;
    for (int i = 0; i < static_cast<int>(craftingRecipes().size()); ++i)
        if (craftingRecipes()[static_cast<std::size_t>(i)].output == Item::Chest)
            chestRecipe = i;
    improved &= chestRecipe >= 0 && fastCraft.recipeCraftable(chestRecipe, true) &&
                !fastCraft.recipeCraftable(chestRecipe, false) &&
                fastCraft.fillRecipe(chestRecipe, true, true) &&
                fastCraft.craftSlot(0).count == 4 &&
                fastCraft.craftingOutput(true).item == Item::Chest &&
                fastCraft.craftOutputToInventory(true) == 4 &&
                fastCraft.count(Item::Chest) == 4 &&
                fastCraft.count(Item::Planks) == 0 &&
                fastCraft.count(Item::BirchPlanks) == 0;
    fastCraft.clear();
    fastCraft.add(Item::BirchPlanks, 8);
    improved &= fastCraft.fillRecipe(chestRecipe, true, false) &&
                fastCraft.craftingOutput(true).item == Item::Chest;
    fastCraft.crafting_[4] = {Item::Stone, 1, 0};
    const int before = fastCraft.count(Item::BirchPlanks);
    improved &= fastCraft.fillRecipe(chestRecipe, true, false) &&
                fastCraft.count(Item::BirchPlanks) == before &&
                fastCraft.count(Item::Stone) == 1 &&
                fastCraft.crafting_[4].empty();
    int furnaceRecipe = -1;
    for (int i = 0; i < static_cast<int>(craftingRecipes().size()); ++i)
        if (craftingRecipes()[static_cast<std::size_t>(i)].output == Item::Furnace)
            furnaceRecipe = i;
    fastCraft.clear();
    for (ItemStack& stack : fastCraft.slots_)
        stack = {Item::Stone, 64, 0};
    fastCraft.slots_[0] = {Item::Cobblestone, 1, 0};
    for (int slot : {1,2,3,5,6,7,8})
        fastCraft.crafting_[static_cast<std::size_t>(slot)] = {Item::Cobblestone, 1, 0};
    improved &= furnaceRecipe >= 0 && fastCraft.recipeCraftable(furnaceRecipe, true) &&
                fastCraft.fillRecipe(furnaceRecipe, true, false) &&
                fastCraft.craftingOutput(true).item == Item::Furnace &&
                fastCraft.count(Item::Cobblestone) == 0 &&
                fastCraft.crafting_[1].count == 1;
    fastCraft.crafting_[4] = {Item::Stone, 1, 0};
    improved &= !fastCraft.fillRecipe(furnaceRecipe, true, false) &&
                fastCraft.crafting_[4].item == Item::Stone;
    improved &= blockToItem(Block::Stone) == Item::Cobblestone &&
                smeltingResult(Item::Cobblestone) == Item::Stone &&
                smeltingResult(Item::Sand) == Item::Glass &&
                smeltingResult(Item::IronOre) == Item::IronIngot &&
                smeltingResult(Item::GoldOre) == Item::GoldIngot &&
                smeltingResult(Item::CopperOre) == Item::CopperIngot &&
                smeltingResult(Item::RawBeef) == Item::CookedBeef &&
                smeltingResult(Item::RawPork) == Item::CookedPork &&
                smeltingResult(Item::RawMutton) == Item::CookedMutton;
    report = ok && improved
                 ? "2x2/3x3 recipes, shifted shapes, tools/mirrors, material groups, autofill, batch output, invalid ingredients, and full inventory passed"
                 : "crafting regression";
    return ok && improved;
}

const std::vector<Item>& creativeCatalog() {
    static const std::vector<Item> catalog = [] {
        std::vector<Item> result;
        for (int value = static_cast<int>(Item::Grass);
             value < static_cast<int>(Item::Count);
             ++value) {
            result.push_back(static_cast<Item>(value));
        }
        return result;
    }();
    return catalog;
}
Block itemToBlock(Item i) {
    switch (i) {
    case Item::Grass:
        return Block::Grass;
    case Item::Dirt:
        return Block::Dirt;
    case Item::Stone:
        return Block::Stone;
    case Item::Sand:
        return Block::Sand;
    case Item::Log:
        return Block::Log;
    case Item::Leaves:
        return Block::Leaves;
    case Item::Water:
        return Block::Water;
    case Item::CoalOre:
        return Block::CoalOre;
    case Item::IronOre:
        return Block::IronOre;
    case Item::GoldOre:
        return Block::GoldOre;
    case Item::CopperOre:
        return Block::CopperOre;
    case Item::DiamondOre:
        return Block::DiamondOre;
    case Item::Planks:
        return Block::Planks;
    case Item::CraftingTable:
        return Block::CraftingTable;
    case Item::Torch:
        return Block::Torch;
    case Item::Cobblestone:
        return Block::Cobblestone;
    case Item::StoneBricks:
        return Block::StoneBricks;
    case Item::Bricks:
        return Block::Bricks;
    case Item::Glass:
        return Block::Glass;
    case Item::Gravel:
        return Block::Gravel;
    case Item::Clay:
        return Block::Clay;
    case Item::Snow:
        return Block::Snow;
    case Item::SnowBlock:
        return Block::SnowBlock;
    case Item::BirchPlanks:
        return Block::BirchPlanks;
    case Item::BirchLog:
        return Block::BirchLog;
    case Item::BirchLeaves:
        return Block::BirchLeaves;
    case Item::Cactus:
        return Block::Cactus;
    case Item::Furnace:
        return Block::Furnace;
    case Item::Bookshelf:
        return Block::Bookshelf;
    case Item::WoodenDoor:
        return Block::WoodenDoor;
    case Item::WoodenSlab:
        return Block::WoodenSlab;
    case Item::StoneSlab:
        return Block::StoneSlab;
    case Item::Granite:
        return Block::Granite;
    case Item::Diorite:
        return Block::Diorite;
    case Item::Andesite:
        return Block::Andesite;
    case Item::MossyCobblestone:
        return Block::MossyCobblestone;
    case Item::MossyStoneBricks:
        return Block::MossyStoneBricks;
    case Item::Ice:
        return Block::Ice;
    case Item::Mud:
        return Block::Mud;
    case Item::TallGrass:
        return Block::TallGrass;
    case Item::RedFlower:
        return Block::RedFlower;
    case Item::YellowFlower:
        return Block::YellowFlower;
    case Item::Ladder:
        return Block::LadderNorth;
    case Item::Chest:
        return Block::Chest;
    default:
        return Block::Air;
    }
}
Item blockToItem(Block b) {
    return blockDefinition(b).drop;
}
bool isTool(Item i) {
    return pick(i) || axe(i) || shovel(i) || isSword(i);
}
bool isSword(Item i) {
    return itemDefinition(i).tool == ToolKind::Sword;
}
bool canHarvestBlock(Item i, Block block) {
    const int requiredTier = blockDefinition(block).requiredHarvestTier;
    return requiredTier == 0 || (pick(i) && tier(i) >= requiredTier);
}
Item smeltingResult(Item item) {
    switch (item) {
    case Item::IronOre:
        return Item::IronIngot;
    case Item::GoldOre:
        return Item::GoldIngot;
    case Item::CopperOre:
        return Item::CopperIngot;
    case Item::RawBeef:
        return Item::CookedBeef;
    case Item::RawPork:
        return Item::CookedPork;
    case Item::RawMutton:
        return Item::CookedMutton;
    case Item::Sand:
        return Item::Glass;
    case Item::Cobblestone:
        return Item::Stone;
    default:
        return Item::None;
    }
}
bool isFood(Item i) {
    return itemDefinition(i).foodValue > 0.0f;
}
float foodValue(Item i) {
    return itemDefinition(i).foodValue;
}
float attackDamage(Item i) {
    return itemDefinition(i).attackDamage;
}
float attackCooldown(Item i) {
    return itemDefinition(i).attackCooldown;
}
float blockHardness(Block b) {
    return blockDefinition(b).hardness;
}
float toolBreakMultiplier(Item i, Block b) {
    const ItemDefinition& item = itemDefinition(i);
    if (item.tier == 0 || item.tool != blockDefinition(b).correctTool)
        return 1.0f;
    if (i == Item::GoldPickaxe || i == Item::GoldAxe || i == Item::GoldShovel)
        return 9.0f;
    return 2.0f + item.tier * 1.5f;
}
glm::vec3 itemColor(Item i) {
    switch (i) {
    case Item::Grass:
        return {.35f, .72f, .25f};
    case Item::Dirt:
        return {.48f, .31f, .16f};
    case Item::Stone:
        return glm::vec3(.5f);
    case Item::Sand:
        return {.86f, .78f, .48f};
    case Item::Log:
        return {.42f, .25f, .1f};
    case Item::Leaves:
        return {.2f, .62f, .18f};
    case Item::Water:
        return {.15f, .42f, .9f};
    case Item::CoalOre:
        return glm::vec3(.18f);
    case Item::IronOre:
        return {.72f, .57f, .45f};
    case Item::GoldOre:
        return {1, .75f, .12f};
    case Item::CopperOre:
        return {.76f, .38f, .2f};
    case Item::DiamondOre:
        return {.15f, .9f, .9f};
    case Item::Planks:
        return {.68f, .47f, .22f};
    case Item::Stick:
        return {.5f, .3f, .12f};
    case Item::CraftingTable:
        return {.62f, .39f, .18f};
    case Item::Torch:
        return {1, .68f, .1f};
    case Item::Seeds:
        return {.38f, .65f, .15f};
    case Item::Wheat:
        return {.92f, .74f, .18f};
    case Item::Bread:
        return {.82f, .52f, .16f};
    case Item::RawMeat:
    case Item::RawBeef:
        return {.8f, .16f, .18f};
    case Item::RawPork:
        return {.95f, .48f, .52f};
    case Item::Wool:
        return glm::vec3(.92f);
    case Item::CookedBeef:
        return {.42f, .18f, .10f};
    case Item::CookedPork:
    case Item::CookedMutton:
        return {.72f, .38f, .25f};
    case Item::Cobblestone:
    case Item::StoneBricks:
    case Item::StoneSlab:
    case Item::Furnace:
        return {.48f, .49f, .51f};
    case Item::Bricks:
        return {.66f, .27f, .18f};
    case Item::Glass:
        return {.66f, .88f, .94f};
    case Item::Gravel:
        return {.43f, .42f, .42f};
    case Item::Clay:
        return {.48f, .53f, .63f};
    case Item::Snow:
    case Item::SnowBlock:
        return {.93f, .97f, 1.0f};
    case Item::BirchPlanks:
        return {.78f, .68f, .45f};
    case Item::BirchLog:
        return {.82f, .80f, .68f};
    case Item::BirchLeaves:
        return {.32f, .68f, .24f};
    case Item::Cactus:
        return {.18f, .52f, .16f};
    case Item::Bookshelf:
        return {.55f, .31f, .16f};
    case Item::WoodenDoor:
    case Item::WoodenSlab:
        return {.64f, .43f, .20f};
    case Item::Coal:
        return {.10f, .10f, .12f};
    case Item::IronIngot:
        return {.82f, .83f, .84f};
    case Item::GoldIngot:
        return {1.0f, .76f, .14f};
    case Item::Diamond:
        return {.18f, .92f, .92f};
    case Item::CopperIngot:
        return {.82f, .38f, .20f};
    case Item::Apple:
        return {.86f, .10f, .10f};
    case Item::Leather:
        return {.52f, .25f, .10f};
    case Item::RawMutton:
        return {.82f, .22f, .25f};
    case Item::Granite:
        return {.59f, .42f, .38f};
    case Item::Diorite:
        return {.78f, .77f, .73f};
    case Item::Andesite:
        return {.45f, .47f, .48f};
    case Item::MossyCobblestone:
    case Item::MossyStoneBricks:
        return {.35f, .48f, .28f};
    case Item::Ice:
        return {.55f, .82f, .95f};
    case Item::Mud:
        return {.30f, .20f, .13f};
    case Item::TallGrass:
        return {.28f, .67f, .16f};
    case Item::RedFlower:
        return {.90f, .16f, .12f};
    case Item::YellowFlower:
        return {.96f, .78f, .10f};
    case Item::Ladder:
    case Item::Chest:
        return {.60f, .38f, .16f};
    default:
        return isTool(i) ? glm::vec3(.72f) : glm::vec3(.7f);
    }
}

SurvivalWorld::SurvivalWorld(std::uint32_t seed) : seed_(seed) {}
bool SurvivalWorld::isBillboard(AnimalType type) {
    const auto id = static_cast<std::uint8_t>(type);
    return id >= 6 && id < 6 + BillboardMobCount;
}
std::size_t SurvivalWorld::billboardIndex(AnimalType type) {
    return static_cast<std::uint8_t>(type) - 6;
}
SurvivalWorld::AnimalType SurvivalWorld::billboardType(std::size_t index) {
    return static_cast<AnimalType>(6 + index);
}
std::string SurvivalWorld::mobName(int index) const {
    if (index < 0 || index >= static_cast<int>(animals_.size())) return {};
    const auto type = animals_[index].type;
    if (isBillboard(type)) return billboardMobName(billboardIndex(type));
    switch (type) {
    case AnimalType::Cow: return "Cow";
    case AnimalType::Pig: return "Pig";
    case AnimalType::Sheep: return "Sheep";
    case AnimalType::Villager: return "Villager";
    case AnimalType::Pillager: return "Pillager";
    default: return {};
    }
}
std::int64_t SurvivalWorld::chunkKey(int x, int z) {
    return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(z);
}
void SurvivalWorld::spawnDrop(const glm::vec3& p,
                              Item item,
                              int amount,
                              int durability,
                              const glm::vec3& velocity,
                              float pickupDelay) {
    if (item == Item::None || amount <= 0)
        return;
    Drop d;
    d.position = p;
    d.velocity = velocity;
    d.stack = {item,
               amount,
               isTool(item) ? (durability < 0 ? Inventory::maxDurability(item) : durability) : 0};
    d.pickupDelay = std::max(0.0f, pickupDelay);
    drops_.push_back(d);
}
void SurvivalWorld::spawnExperience(const glm::vec3& p, int amount) {
    int n = std::max(1, amount / 2);
    for (int i = 0; i < n; ++i)
        experienceOrbs_.push_back({p + glm::vec3((i % 3 - .8f) * .12f, .2f, (i % 2 - .5f) * .15f),
                                   {0, .7f, 0},
                                   std::max(1, amount / n),
                                   0});
}
void SurvivalWorld::spawnStructureMob(const glm::ivec3& position, bool hostile) {
    const std::uint64_t id = structureMobMarkerId(
        position.x, position.y, position.z, hostile);
    if (!spawnedStructureMarkers_.insert(id).second)
        return;
    Animal animal;
    animal.type = hostile ? AnimalType::Pillager : AnimalType::Villager;
    animal.position = glm::vec3(position) + glm::vec3(0.5f, 0.01f, 0.5f);
    animal.home = animal.position;
    animal.health = hostile ? 14.0f : 20.0f;
    animal.persistent = true;
    animal.thinkTimer = 0.5f;
    animals_.push_back(animal);
}
bool SurvivalWorld::summonMob(const std::string& name, const glm::vec3& position) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    Animal mob;
    if (normalized == "cow") mob.type = AnimalType::Cow;
    else if (normalized == "pig") mob.type = AnimalType::Pig;
    else if (normalized == "sheep") mob.type = AnimalType::Sheep;
    else if (normalized == "villager") mob.type = AnimalType::Villager;
    else if (normalized == "pillager") mob.type = AnimalType::Pillager;
    else {
        bool found = false;
        const auto& definitions = billboardMobDefinitions();
        for (std::size_t i = 0; i < definitions.size(); ++i) {
            std::string candidate = definitions[i].name;
            std::transform(candidate.begin(), candidate.end(), candidate.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (candidate == normalized) {
                mob.type = billboardType(i);
                mob.health = definitions[i].health;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    mob.position = position;
    mob.home = position;
    mob.persistent = true;
    mob.thinkTimer = 0.5f;
    if (mob.type == AnimalType::Villager) mob.health = 20.0f;
    else if (mob.type == AnimalType::Pillager) mob.health = 14.0f;
    animals_.push_back(mob);
    return true;
}
void SurvivalWorld::spawnBillboardPreview(const glm::vec3& origin,
                                          const glm::vec3& forward) {
    glm::vec2 horizontal(forward.x, forward.z);
    if (glm::dot(horizontal, horizontal) < .01f) horizontal = {0.0f, -1.0f};
    horizontal = glm::normalize(horizontal);
    const glm::vec2 side(-horizontal.y, horizontal.x);
    for (std::size_t index = 0; index < BillboardMobCount; ++index) {
        Animal mob;
        mob.type = billboardType(index);
        mob.position = origin + glm::vec3(horizontal.x * 6.0f + side.x * (index - 1.5f) * 1.4f,
                                          0.0f,
                                          horizontal.y * 6.0f + side.y * (index - 1.5f) * 1.4f);
        mob.home = mob.position;
        mob.health = billboardMobDefinitions()[index].health;
        mob.thinkTimer = 8.0f;
        animals_.push_back(mob);
    }
}
void SurvivalWorld::spawnNearbyAnimals(World& world, const glm::vec3& playerPosition,
                                       float daylight) {
    // One bounded batch every five seconds; eight extra nighttime candidates
    // use the same hostile roll as the base sixteen. Expected hostile attempts
    // rise from 16*.52 to 24*.52 (1.5x), without changing passive attempts.
    const float maximumDistance = std::min(
        112.0f, static_cast<float>(world.simulationDistance() * CHUNK_SIZE - 5));
    if (maximumDistance <= 25.0f)
        return;
    std::mt19937 random(seed_ ^ (++spawnSequence_ * 0x9e3779b9U));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::array<double, BillboardMobCount> billboardWeights{};
    for (std::size_t index = 0; index < BillboardMobCount; ++index)
        billboardWeights[index] = billboardMobDefinitions()[index].spawnWeight;
    std::discrete_distribution<std::size_t> billboardChoice(
        billboardWeights.begin(), billboardWeights.end());
    int passiveCount = 0;
    int hostileCount = 0;
    for (const Animal& animal : animals_) {
        if (animal.deathTimer > 0)
            continue;
        (isBillboard(animal.type) || animal.type == AnimalType::Pillager
            ? hostileCount : passiveCount)++;
    }
    spawnAttemptsLastTick_ = 0;
    spawnSuccessesLastTick_ = 0;
    int passiveSpawned = 0;
    const bool night = daylight <= 0.01f;
    const int candidates = NaturalSpawnCandidates +
        (night ? NightHostileBonusCandidates : 0);
    for (int attempt = 0; attempt < candidates; ++attempt) {
        ++spawnAttemptsLastTick_;
        const float angle = unit(random) * 6.2831853f;
        const float radius = std::sqrt(24.0f * 24.0f +
            unit(random) * (maximumDistance * maximumDistance - 24.0f * 24.0f));
        const int x = static_cast<int>(std::floor(playerPosition.x + std::cos(angle) * radius));
        const int z = static_cast<int>(std::floor(playerPosition.z + std::sin(angle) * radius));
        if (!world.simulationActiveAt(static_cast<float>(x), static_cast<float>(z)))
            continue;
        const bool hostile = unit(random) < HostileCandidateChance;
        if (attempt >= NaturalSpawnCandidates && !hostile)
            continue; // Bonus candidates never replace passive spawns.
        if (hostile && daylight > 0.01f)
            continue;
        if (hostile ? hostileCount >= HostileMobCap
                    : passiveCount >= PassiveMobCap || passiveSpawnCooldown_ > 0.0f ||
                      passiveSpawned >= 3)
            continue;
        const int terrainY = world.terrainHeight(x, z);
        int feetY = terrainY + 1;
        if (feetY < 3 || feetY >= WORLD_HEIGHT - 2)
            continue;
        // Check the terrain surface and a few blocks below it without scanning the column.
        bool valid = false;
        Block ground = Block::Air;
        for (int probe = 0; probe < 4; ++probe) {
            const int candidateY = feetY - probe;
            ground = world.getBlock(x, candidateY - 1, z);
            const BlockGeometryProperties shape = blockGeometry(ground);
            if (!isSolid(ground) || blockDefinition(ground).transparent ||
                shape.shape != BlockShape::Cube || ground == Block::Cactus)
                continue;
            const Block feet = world.getBlock(x, candidateY, z);
            const Block head = world.getBlock(x, candidateY + 1, z);
            if (isSolid(feet) || isWater(feet) || isSolid(head) || isWater(head))
                continue;
            feetY = candidateY;
            valid = true;
            break;
        }
        if (!valid)
            continue;
        if (hostile && billboardTouchesSolid(
                world, {x + .5f, feetY + .01f, z + .5f}))
            continue;
        if (!hostile) {
            if (ground != Block::Grass || feetY <= SEA_LEVEL ||
                world.biomeNameAt(x, z) == "DESERT")
                continue;
            const std::int64_t key = chunkKey(
                static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_SIZE)),
                static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_SIZE)));
            if (spawnedChunks_.find(key) != spawnedChunks_.end())
                continue;
            spawnedChunks_.insert(key);
            ++passiveSpawned;
        } else {
            const float blockLight = static_cast<float>(world.blockLightAt(x, feetY + 1, z));
            const float skyLight = static_cast<float>(world.sunlightAt(x, feetY + 1, z));
            if (std::max(blockLight, skyLight * daylight) > 6.0f)
                continue;
        }
        Animal animal;
        animal.position = {x + .5f, feetY + .01f, z + .5f};
        animal.type = hostile ? billboardType(billboardChoice(random))
                      : static_cast<AnimalType>(static_cast<int>(unit(random) * 3.0f));
        animal.health = isBillboard(animal.type)
                        ? billboardMobDefinitions()[billboardIndex(animal.type)].health
                        : animal.type == AnimalType::Cow ? 10.0f : 8.0f;
        animal.thinkTimer = unit(random) * 2.0f;
        animals_.push_back(animal);
        ++spawnSuccessesLastTick_;
        (hostile ? hostileCount : passiveCount)++;
    }
    if (passiveSpawned > 0)
        passiveSpawnCooldown_ = 20.0f;
}
int SurvivalWorld::targetedAnimal(const glm::vec3& o, const glm::vec3& d, float reach) const {
    int best = -1;
    float bestT = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(animals_.size()); ++i) {
        const auto& a = animals_[i];
        if (a.deathTimer > 0)
            continue;
        float t;
        const bool billboard = isBillboard(a.type);
        const bool hit = billboard
            ? rayBox(o, d, a.position + glm::vec3(-BillboardMobHalfWidth, 0, -BillboardMobHalfWidth),
                     a.position + glm::vec3(BillboardMobHalfWidth, BillboardMobHeight,
                                            BillboardMobHalfWidth), reach, t)
            : raySphere(o, d, a.position + glm::vec3(0, .55f, 0),
                        reach, a.age < 0 ? .38f : .68f, t);
        if (hit &&
            t < bestT) {
            best = i;
            bestT = t;
        }
    }
    return best;
}
MobTarget SurvivalWorld::raycastMob(const glm::vec3& o,
                                    const glm::vec3& d,
                                    float reach,
                                    const World& w,
                                    float blockDistance) const {
    MobTarget result;
    int i = targetedAnimal(o, d, std::min(reach, blockDistance));
    if (i < 0)
        return result;
    float along = 0;
    const bool billboard = isBillboard(animals_[i].type);
    glm::vec3 center = animals_[i].position + glm::vec3(
        0, billboard ? BillboardMobHeight * .5f : .55f, 0);
    if (billboard)
        rayBox(o, d, animals_[i].position +
               glm::vec3(-BillboardMobHalfWidth, 0, -BillboardMobHalfWidth),
               animals_[i].position +
               glm::vec3(BillboardMobHalfWidth, BillboardMobHeight, BillboardMobHalfWidth),
               reach, along);
    else
        raySphere(o, d, center, reach, .7f, along);
    RayHit obstruction;
    if (w.raycast(o, d, along, obstruction) && obstruction.distance + 0.05f < along)
        return result;
    result = {i, along, center};
    return result;
}
AttackResult
SurvivalWorld::attackMob(int index, Item held, bool critical, const glm::vec3& attacker) {
    AttackResult r;
    if (index < 0 || index >= static_cast<int>(animals_.size()))
        return r;
    Animal& a = animals_[index];
    if (a.deathTimer > 0 || a.hurtCooldown > 0)
        return r;
    r.hit = true;
    r.critical = critical;
    r.damage = attackDamage(held) * (critical ? 1.5f : 1.f);
    r.position = a.position + glm::vec3(
        0, isBillboard(a.type) ? BillboardMobHeight * .5f : .55f, 0);
    a.health -= r.damage;
    a.hurtCooldown = .34f;
    a.hurtFlash = .22f;
    glm::vec3 push = a.position - attacker;
    push.y = 0;
    if (glm::dot(push, push) < .001f)
        push = {1, 0, 0};
    else
        push = glm::normalize(push);
    a.velocity += push * (critical ? 6.f : 4.4f);
    a.velocity.y = critical ? 4.2f : 3.f;
    if (isBillboard(a.type) || a.type == AnimalType::Pillager) {
        a.angerTimer = 18.0f;
        a.rememberedTarget = attacker;
        a.memoryTimer = 6.0f;
    } else {
        a.fleeTimer = 4.0f;
        a.rememberedTarget = attacker;
    }
    a.thinkTimer = 0.0f;
    if (sounds_) {
        const MobSoundType type = mobSoundType(static_cast<std::uint8_t>(a.type));
        if (a.health <= 0.0f) sounds_->playMobDeath(type, a.position);
        else sounds_->playMobHurt(type, a.position);
    }
    if (a.health <= 0) {
        a.deathTimer = .65f;
        r.killed = true;
        releaseDrops(a);
        spawnExperience(a.position, 4);
    }
    return r;
}
void SurvivalWorld::releaseDrops(Animal& a) {
    if (a.dropsReleased)
        return;
    a.dropsReleased = true;
    if (a.type == AnimalType::Cow) {
        spawnDrop(a.position, Item::RawBeef, 1 + static_cast<int>(std::abs(a.position.x)) % 2);
        spawnDrop(a.position, Item::Leather, 1);
    } else if (a.type == AnimalType::Pig) {
        spawnDrop(a.position, Item::RawPork, 1 + static_cast<int>(std::abs(a.position.z)) % 2);
    } else if (a.type == AnimalType::Sheep) {
        spawnDrop(a.position, Item::Wool, 1);
        spawnDrop(a.position, Item::RawMutton, 1 + static_cast<int>(std::abs(a.position.x)) % 2);
    }
}
RenderCuboid SurvivalWorld::mobOutline(int i) const {
    if (i < 0 || i >= static_cast<int>(animals_.size()))
        return {};
    const Animal& a = animals_[i];
    if (isBillboard(a.type))
        return {a.position + glm::vec3(0, BillboardMobHeight * .5f, 0),
                glm::vec3(BillboardMobHalfWidth * 2, BillboardMobHeight,
                          BillboardMobHalfWidth * 2), {1, 1, 1}};
    float baby = a.age < 0 ? .55f : 1;
    return {
        a.position + glm::vec3(0, .53f * baby, 0), glm::vec3(1.08f, .95f, .66f) * baby, {1, 1, 1}};
}
bool SurvivalWorld::feedAnimal(const glm::vec3& o, const glm::vec3& d, Item food) {
    int i = targetedAnimal(o, d, 5);
    if (i < 0)
        return false;
    Animal& a = animals_[i];
    bool ok = (a.type == AnimalType::Cow || a.type == AnimalType::Sheep)
                  ? food == Item::Wheat
                  : a.type == AnimalType::Pig && food == Item::Seeds;
    if (!ok || a.breedingCooldown > 0 || a.age < 0)
        return false;
    a.loveTimer = 12;
    for (int j = 0; j < static_cast<int>(animals_.size()); ++j)
        if (j != i && animals_[j].type == a.type && animals_[j].loveTimer > 0 &&
            animals_[j].breedingCooldown <= 0 && animals_[j].age >= 0 &&
            glm::distance(animals_[j].position, a.position) < 5) {
            Animal baby = a;
            baby.position = (a.position + animals_[j].position) * .5f;
            baby.age = -60;
            baby.loveTimer = 0;
            baby.breedingCooldown = 0;
            baby.health = a.type == AnimalType::Cow ? 10.f : 8.f;
            baby.persistent = true;
            animals_[j].loveTimer = 0;
            animals_[j].breedingCooldown = 45;
            a.loveTimer = 0;
            a.breedingCooldown = 45;
            animals_.push_back(baby);
            spawnExperience(baby.position, 3);
            break;
        }
    return true;
}

bool SurvivalWorld::canSeePlayer(const Animal& animal, const World& world,
                                  const Player& player, float maximumDistance) const {
    const glm::vec3 from = animal.position + glm::vec3(0.0f, 0.8f, 0.0f);
    const glm::vec3 to = player.position() + glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 delta = to - from;
    const float distance = glm::length(delta);
    if (distance > maximumDistance || distance < 0.001f)
        return false;
    const glm::vec3 step = delta / distance * 0.28f;
    glm::vec3 point = from;
    for (float traveled = 0.28f; traveled < distance - 0.25f; traveled += 0.28f) {
        point += step;
        const Block block = world.getBlock(static_cast<int>(std::floor(point.x)),
                                           static_cast<int>(std::floor(point.y)),
                                           static_cast<int>(std::floor(point.z)));
        if (isSolid(block) && !isDoorOpen(block) && !isLeaf(block) && block != Block::Glass &&
            block != Block::Ice)
            return false;
    }
    return true;
}

void SurvivalWorld::detonate(const glm::vec3& center, World& world, Player& player) {
    const Explosion blast{center, 3.0f, 3.0f, 12.0f, true};
    const glm::vec3 playerCenter = player.position() + glm::vec3(0, .9f, 0);
    const glm::vec3 delta = playerCenter - center;
    const float distance = glm::length(delta);
    if (!player.isCreative() && !player.isSpectator() && distance < blast.radius) {
        const float exposure = explosionExposure(world, center, playerCenter);
        const float damage = explosionDamage(blast, distance, exposure);
        if (damage > 0.0f) {
            player.damage(damage);
            player.applyImpulse(glm::normalize(delta + glm::vec3(0, .25f, 0)) *
                                std::min(8.0f, damage * .8f));
        }
    }
    for (Animal& other : animals_) {
        if (other.deathTimer != 0.0f) continue;
        const glm::vec3 target = other.position + glm::vec3(
            0, isBillboard(other.type) ? BillboardMobHeight * .5f : .75f, 0);
        const float separation = glm::length(target - center);
        if (separation >= blast.radius || separation < .01f) continue;
        const float damage = explosionDamage(
            blast, separation, explosionExposure(world, center, target));
        if (damage <= 0.0f) continue;
        other.health -= damage;
        other.velocity += glm::normalize(target - center) * std::min(6.0f, damage);
        other.hurtFlash = .22f;
        if (other.health <= 0.0f) {
            other.deathTimer = .65f;
            releaseDrops(other);
            if (sounds_)
                sounds_->playMobDeath(mobSoundType(static_cast<std::uint8_t>(other.type)),
                                      other.position);
        }
    }
    int releasedDrops = 0;
    damageExplosionTerrain(world, blast, [&](const glm::ivec3& cell, Block block) {
        const glm::vec3 dropPosition = glm::vec3(cell) + glm::vec3(.5f);
        for (const ItemStack& stack : world.takeBlockEntityContents(cell))
            if (stack.item != Item::None && stack.count > 0)
                spawnDrop(dropPosition, stack.item, stack.count, stack.durability);
        const Item item = blockDefinition(block).drop;
        if (item != Item::None && (!isDoorUpper(block)) && releasedDrops < 64) {
            spawnDrop(dropPosition, item);
            ++releasedDrops;
        }
    });
    explosionEffects_.push_back(center);
}

void SurvivalWorld::updateArrows(float dt, World& world, Player& player) {
    for (Arrow& arrow : arrows_) {
        if (!arrow.active) continue;
        arrow.age += dt;
        if (arrow.age > 8.0f || !world.simulationActiveAt(arrow.position.x, arrow.position.z) ||
            glm::distance(arrow.position, player.position()) > 110.0f) {
            arrow.active = false;
            continue;
        }
        const int steps = std::max(1, static_cast<int>(std::ceil(dt / .025f)));
        const float step = dt / steps;
        for (int index = 0; index < steps && arrow.active; ++index) {
            const glm::vec3 movement = arrow.velocity * step;
            const float length = glm::length(movement);
            RayHit obstacle;
            const bool blockHit = length > .0001f &&
                world.raycast(arrow.position, movement / length, length, obstacle);
            float playerHitDistance = 0.0f;
            const bool playerHit = !player.isCreative() && !player.isSpectator() &&
                !player.isDead() && length > .0001f &&
                rayBox(arrow.position, movement / length, player.aabbMinimum(),
                       player.aabbMaximum(), length, playerHitDistance);
            if (blockHit && (!playerHit || obstacle.distance <= playerHitDistance)) {
                arrow.active = false;
                break;
            }
            if (playerHit) {
                player.damage(3.0f, PlayerDamageSource::Projectile);
                player.applyImpulse(glm::normalize(movement) * 1.8f);
                arrow.active = false;
                break;
            }
            arrow.position += movement;
            arrow.velocity.y -= 8.0f * step;
        }
    }
    arrows_.erase(std::remove_if(arrows_.begin(), arrows_.end(),
        [](const Arrow& arrow) { return !arrow.active; }), arrows_.end());
}

std::vector<glm::vec3> SurvivalWorld::takeExplosionEffects() {
    std::vector<glm::vec3> result;
    result.swap(explosionEffects_);
    return result;
}

bool SurvivalWorld::canNavigateTo(const Animal& animal, const World& world,
                                   const glm::vec2& direction) const {
    if (glm::dot(direction, direction) < 0.001f)
        return true;
    const glm::vec2 target(animal.position.x + direction.x * 1.2f,
                           animal.position.z + direction.y * 1.2f);
    const int x = static_cast<int>(std::floor(target.x));
    const int z = static_cast<int>(std::floor(target.y));
    const int currentFeet = static_cast<int>(std::floor(animal.position.y + 0.05f));
    const bool billboard = isBillboard(animal.type);
    if (billboard) {
        float surface = 0.0f;
        return billboardFooting(world, target, animal.position.y, -1.05f, 1.02f, surface);
    }
    for (int rise : {0, 1, -1}) {
        const int feetY = currentFeet + rise;
        const Block ground = world.getBlock(x, feetY - 1, z);
        const BlockGeometryProperties shape = blockGeometry(ground);
        if (!isSolid(ground) || shape.shape != BlockShape::Cube ||
            blockDefinition(ground).transparent || ground == Block::Cactus)
            continue;
        const Block feet = world.getBlock(x, feetY, z);
        const Block head = world.getBlock(x, feetY + 1, z);
        if ((!isSolid(feet) || isDoorOpen(feet)) && !isWater(feet) &&
            feet != Block::Cactus &&
            (!isSolid(head) || isDoorOpen(head)) && !isWater(head) &&
            head != Block::Cactus)
            return true;
    }
    return false;
}

glm::vec2 SurvivalWorld::chooseNavigationHeading(Animal& animal, World& world,
                                                  const glm::vec2& desired) {
    ++navigationQueriesLastTick_;
    if (glm::dot(desired, desired) < 0.001f)
        return {0.0f, 0.0f};
    const glm::vec2 forward = glm::normalize(desired);
    constexpr std::array<float, 8> turns{0.0f, 0.60f, -0.60f, 1.18f,
                                          -1.18f, 1.75f, -1.75f, 3.14159f};
    float bestScore = -100.0f;
    glm::vec2 best(0.0f);
    for (float turn : turns) {
        const float cosine = std::cos(turn);
        const float sine = std::sin(turn);
        const glm::vec2 candidate(forward.x * cosine - forward.y * sine,
                                   forward.x * sine + forward.y * cosine);
        if (!canNavigateTo(animal, world, candidate))
            continue;
        const float score = glm::dot(candidate, forward) +
                            glm::dot(candidate, animal.heading) * 0.14f;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}
void SurvivalWorld::updateAnimal(Animal& a, float dt, World& w, Player& player,
                                 const Inventory& inventory, float daylight) {
    (void)daylight;
    a.attackCooldown = std::max(0.f, a.attackCooldown - dt);
    a.hurtCooldown = std::max(0.f, a.hurtCooldown - dt);
    a.hurtFlash = std::max(0.f, a.hurtFlash - dt);
    a.loveTimer = std::max(0.f, a.loveTimer - dt);
    a.breedingCooldown = std::max(0.f, a.breedingCooldown - dt);
    a.age = std::min(0.f, a.age + dt);
    if (a.deathTimer > 0) {
        a.deathTimer -= dt;
        a.position.y -= dt * .35f;
        if (a.deathTimer <= 0)
            a.deathTimer = -.001f;
        return;
    }
    a.thinkTimer -= dt;
    a.fleeTimer = std::max(0.0f, a.fleeTimer - dt);
    a.angerTimer = std::max(0.0f, a.angerTimer - dt);
    a.memoryTimer = std::max(0.0f, a.memoryTimer - dt);
    a.idleTimer = std::max(0.0f, a.idleTimer - dt);
    a.lungeCooldown = std::max(0.0f, a.lungeCooldown - dt);
    a.stepJumpCooldown = std::max(0.0f, a.stepJumpCooldown - dt);
    const glm::vec3 delta = player.position() - a.position;
    const float distance = glm::length(delta);
    const BillboardMobDefinition* definition = isBillboard(a.type)
        ? &billboardMobDefinitions()[billboardIndex(a.type)] : nullptr;
    const bool silentExploder = definition &&
        definition->behavior == BillboardBehavior::ExplosiveChase;
    if (sounds_ && distance < 25.0f && !silentExploder) {
        if (a.ambientSoundTimer == 3.0f) {
            a.ambientSoundTimer +=
                std::abs(std::sin(a.position.x * 7.13f + a.position.z * 13.61f)) * 7.0f;
        }
        a.ambientSoundTimer -= dt;
        if (a.ambientSoundTimer <= 0.0f) {
            sounds_->playMobAmbient(mobSoundType(static_cast<std::uint8_t>(a.type)), a.position);
            a.ambientSoundTimer = 5.0f +
                std::abs(std::sin(a.position.x * 5.71f + a.position.z * 3.17f + a.age)) * 8.0f;
        }
        a.stepSoundTimer -= dt;
        if (a.grounded && glm::dot(a.heading, a.heading) > .1f &&
            a.stepSoundTimer <= 0.0f) {
            sounds_->playMobStep(mobSoundType(static_cast<std::uint8_t>(a.type)), a.position);
            a.stepSoundTimer = .43f;
        }
    }
    const bool hostile = !player.isCreative() && !player.isSpectator() &&
        (a.type == AnimalType::Pillager ||
         isBillboard(a.type));
    const Item heldFood = inventory.selectedItem();
    const bool preferredFood = !player.isSpectator() &&
        (a.type == AnimalType::Cow || a.type == AnimalType::Sheep
                                   ? heldFood == Item::Wheat
                               : a.type == AnimalType::Pig ? heldFood == Item::Seeds
                               : false);
    if (a.thinkTimer <= 0.0f) {
        const float detectionRange = definition ? definition->detectionRange :
                                     hostile ? 14.0f : 8.0f;
        const bool visible = distance < detectionRange &&
                             canSeePlayer(a, w, player, detectionRange);
        glm::vec2 desired(0.0f);
        bool active = false;
        if (a.type == AnimalType::Villager) {
            for (const Animal& nearby : animals_) {
                if ((!isBillboard(nearby.type) &&
                     nearby.type != AnimalType::Pillager) || nearby.deathTimer > 0.0f)
                    continue;
                const glm::vec2 threat(nearby.position.x - a.position.x,
                                       nearby.position.z - a.position.z);
                if (glm::dot(threat, threat) < 49.0f) {
                    a.rememberedTarget = nearby.position;
                    a.fleeTimer = std::max(a.fleeTimer, 2.0f);
                    break;
                }
            }
        }
        if (hostile && visible) {
            a.rememberedTarget = player.position();
            a.memoryTimer = definition &&
                definition->behavior == BillboardBehavior::MeleeChase ? 4.0f : 3.0f;
        }
        if (a.fleeTimer > 0.0f) {
            desired = glm::vec2(a.position.x - a.rememberedTarget.x,
                                a.position.z - a.rememberedTarget.z);
            active = true;
        } else if (hostile && a.memoryTimer > 0.0f) {
            desired = glm::vec2(a.rememberedTarget.x - a.position.x,
                                a.rememberedTarget.z - a.position.z);
            if (definition && definition->behavior == BillboardBehavior::RangedAttack &&
                visible) {
                if (distance < 5.0f) desired = -desired;
                else if (distance < 12.0f) desired = glm::vec2(0);
            }
            active = true;
        } else if (!hostile && preferredFood && visible) {
            desired = glm::vec2(delta.x, delta.z);
            active = true;
        } else if ((a.type == AnimalType::Villager ||
                    a.type == AnimalType::Pillager) &&
                   glm::distance(glm::vec2(a.position.x, a.position.z),
                                 glm::vec2(a.home.x, a.home.z)) > 12.0f) {
            desired = glm::vec2(a.home.x - a.position.x, a.home.z - a.position.z);
            active = true;
        } else if (a.idleTimer <= 0.0f) {
            a.wanderPhase += 1.0f;
            const float phase = std::abs(std::sin(a.position.x * 11.7f +
                a.position.z * 7.3f + a.wanderPhase * 2.17f));
            if (phase < 0.24f) {
                a.idleTimer = 1.5f + phase * 4.0f;
            } else {
                const float angle = phase * 31.4f;
                desired = {std::cos(angle), std::sin(angle)};
                active = true;
            }
        }
        if (active && glm::dot(desired, desired) > 0.01f)
            a.heading = chooseNavigationHeading(a, w, desired);
        else
            a.heading = {0.0f, 0.0f};
        if (definition && definition->behavior == BillboardBehavior::WallClimbLunge &&
            hostile && a.memoryTimer > 0.0f &&
            glm::dot(a.heading, a.heading) < .01f &&
            glm::dot(desired, desired) > .01f) {
            const glm::vec2 direction = glm::normalize(desired);
            const glm::vec3 ahead = a.position +
                glm::vec3(direction.x * .45f, 0, direction.y * .45f);
            const bool supported = w.isSolidAt(
                static_cast<int>(std::floor(a.position.x)),
                static_cast<int>(std::floor(a.position.y - .1f)),
                static_cast<int>(std::floor(a.position.z)));
            if (supported && billboardTouchesSolid(w, ahead))
                a.heading = direction;
        }
        a.thinkTimer = hostile && a.memoryTimer > 0.0f ? 0.28f
                     : active ? 0.65f : 1.8f;
    }
    if (hostile && silentExploder) {
        if (distance < 3.0f && canSeePlayer(a, w, player, 4.0f))
            a.fuseTimer += dt;
        else
            a.fuseTimer = std::max(0.0f, a.fuseTimer - dt * 2.0f);
        if (a.fuseTimer > 0.0f)
            a.hurtFlash = std::max(a.hurtFlash, .08f + .12f * a.fuseTimer / 1.5f);
        if (a.fuseTimer >= 1.5f) {
            a.deathTimer = -.001f;
            detonate(a.position + glm::vec3(0, BillboardMobHeight * .5f, 0), w, player);
            return;
        }
    }
    if (hostile && definition && definition->behavior == BillboardBehavior::RangedAttack &&
        distance < definition->detectionRange && a.attackCooldown <= 0.0f &&
        canSeePlayer(a, w, player, definition->detectionRange) && arrows_.size() < 64) {
        const glm::vec3 origin = a.position + glm::vec3(0, BillboardMobHeight * .75f, 0);
        const float travel = distance / 17.0f;
        glm::vec3 aim = player.position() + glm::vec3(0, 1.05f, 0) +
                        player.velocity() * std::min(.35f, travel * .35f);
        aim.y += 4.0f * travel * travel;
        const float spread = .02f + distance * .004f;
        a.wanderPhase += 1.0f;
        aim.x += std::sin(a.position.x * 7.1f + a.wanderPhase * 3.7f) * spread;
        aim.z += std::cos(a.position.z * 9.3f + a.wanderPhase * 2.1f) * spread;
        arrows_.push_back({origin, glm::normalize(aim - origin) * 17.0f,
                           0.0f, true, static_cast<std::uint8_t>(a.type)});
        a.attackCooldown = 2.4f;
    }
    if (hostile && definition && definition->behavior == BillboardBehavior::WallClimbLunge &&
        a.grounded && a.lungeCooldown <= 0.0f && distance >= 2.5f && distance <= 5.0f &&
        canSeePlayer(a, w, player, 5.5f)) {
        glm::vec2 toward(delta.x, delta.z);
        if (glm::dot(toward, toward) > .01f) {
            toward = glm::normalize(toward);
            a.velocity.x += toward.x * 5.0f;
            a.velocity.z += toward.y * 5.0f;
            a.velocity.y = 6.0f;
            a.grounded = false;
            a.lungeCooldown = 2.2f;
        }
    }
    if (hostile && distance < 1.5f && a.attackCooldown <= 0.0f &&
        (!definition || (definition->behavior != BillboardBehavior::ExplosiveChase &&
                         definition->behavior != BillboardBehavior::RangedAttack)) &&
        canSeePlayer(a, w, player)) {
        const float previousHealth = player.health();
        player.damage(isBillboard(a.type)
                          ? billboardMobDefinitions()[billboardIndex(a.type)].damage : 2.0f);
        if (definition && player.health() < previousHealth && distance > .01f)
            player.applyImpulse(glm::normalize(delta + glm::vec3(0, .15f, 0)) * 1.6f);
        a.attackCooldown = isBillboard(a.type)
            ? billboardMobDefinitions()[billboardIndex(a.type)].attackCooldown : 1.25f;
    }
    const float speed = a.fleeTimer > 0.0f ? 2.65f
                        : hostile && a.memoryTimer > 0.0f
                            ? (isBillboard(a.type)
                                   ? billboardMobDefinitions()[billboardIndex(a.type)].chaseSpeed
                                   : 2.35f)
                        : preferredFood && distance < 8.0f ? 1.15f : 0.75f;
    const float movementScale = a.fuseTimer > 0.0f ? .12f : 1.0f;
    glm::vec3 move(a.heading.x * speed * movementScale + a.velocity.x, 0,
                   a.heading.y * speed * movementScale + a.velocity.z);
    glm::vec3 next = a.position + move * dt;
    const bool billboard = isBillboard(a.type);
    int fy = static_cast<int>(std::floor(a.position.y + .05f));
    if (a.grounded && glm::dot(a.heading, a.heading) > 0.01f &&
        !canNavigateTo(a, w, a.heading) &&
        !(definition && definition->behavior == BillboardBehavior::WallClimbLunge && hostile)) {
        next.x = a.position.x;
        next.z = a.position.z;
        a.thinkTimer = 0.0f;
    }
    int ax = static_cast<int>(std::floor(next.x)), az = static_cast<int>(std::floor(next.z));
    if (!billboard && w.isSolidAt(ax, fy, az) &&
        !isDoorOpen(w.getBlock(ax, fy, az))) {
        if ((!w.isSolidAt(ax, fy + 1, az) ||
             isDoorOpen(w.getBlock(ax, fy + 1, az))) &&
            (!w.isSolidAt(ax, fy + 2, az) ||
             isDoorOpen(w.getBlock(ax, fy + 2, az))))
            next.y += 1;
        else {
            if (!(definition && definition->behavior == BillboardBehavior::WallClimbLunge &&
                  hostile)) {
                next.x = a.position.x;
                next.z = a.position.z;
            }
            a.thinkTimer = 0.0f;
        }
    }
    a.velocity.x *= std::pow(.08f, dt);
    a.velocity.z *= std::pow(.08f, dt);
    if (billboard && hostile && a.grounded && a.stepJumpCooldown <= 0.0f &&
        a.memoryTimer > 0.0f && glm::dot(a.heading, a.heading) > .01f &&
        billboardTouchesSolid(w, glm::vec3(next.x, a.position.y, next.z))) {
        const glm::vec2 probe(a.position.x + a.heading.x * .95f,
                              a.position.z + a.heading.y * .95f);
        float stepSurface = 0.0f;
        if (billboardFooting(w, probe, a.position.y, .08f, 1.02f, stepSurface) &&
            !billboardTouchesSolid(w, a.position + glm::vec3(0, .25f, 0)) &&
            !billboardTouchesSolid(w, a.position +
                                   glm::vec3(0, stepSurface - a.position.y + .02f, 0))) {
            a.velocity.y = std::max(a.velocity.y, 6.8f);
            a.grounded = false;
            a.stepJumpCooldown = .7f;
        }
    }
    a.velocity.y -= 18 * dt;
    next.y += a.velocity.y * dt;
    if (billboard) {
        if (a.velocity.y > 0.0f &&
            billboardTouchesSolid(w, glm::vec3(a.position.x, next.y, a.position.z))) {
            next.y = a.position.y;
            a.velocity.y = 0.0f;
        }
        float support = 0.0f;
        if (a.velocity.y <= 0.0f &&
            billboardFooting(w, {next.x, next.z}, a.position.y, -1.05f, .05f, support) &&
            next.y <= support + .02f) {
            next.y = support + .01f;
            a.velocity.y = 0.0f;
            a.grounded = true;
        } else {
            a.grounded = false;
        }
    } else {
        const int bx = static_cast<int>(std::floor(next.x));
        const int bz = static_cast<int>(std::floor(next.z));
        const int below = static_cast<int>(std::floor(next.y - .08f));
        if (w.isSolidAt(bx, below, bz) && !isDoorOpen(w.getBlock(bx, below, bz))) {
            next.y = below + 1.01f;
            a.velocity.y = 0;
            a.grounded = true;
        } else {
            a.grounded = false;
        }
    }
    if (billboard && billboardTouchesSolid(w, next)) {
        if (definition && definition->behavior == BillboardBehavior::WallClimbLunge &&
            hostile && a.memoryTimer > 0.0f && glm::dot(a.heading, a.heading) > .01f &&
            !billboardTouchesSolid(w, a.position + glm::vec3(0, .12f, 0))) {
            a.velocity.y = std::max(a.velocity.y, 3.2f);
            next.y = a.position.y + a.velocity.y * dt;
            if (billboardTouchesSolid(w, glm::vec3(a.position.x, next.y, a.position.z)))
                next.y = a.position.y;
        }
        next.x = a.position.x;
        next.z = a.position.z;
        // A blocked horizontal move returns to the previous X/Z. Resolve the
        // landing there as well, or a stair can make the mob fall through the
        // floor it was already standing on.
        float support = 0.0f;
        if (a.velocity.y <= 0.0f &&
            billboardFooting(w, {next.x, next.z}, a.position.y, -1.05f, .05f, support) &&
            next.y <= support + .02f) {
            next.y = support + .01f;
            a.velocity.y = 0.0f;
            a.grounded = true;
        }
        a.thinkTimer = 0.0f;
    }
    a.position = next;
    const float halfWidth = billboard ? BillboardMobHalfWidth : .38f;
    const glm::vec3 mobMinimum = a.position + glm::vec3(-halfWidth, 0.0f, -halfWidth);
    const glm::vec3 mobMaximum = a.position +
        glm::vec3(halfWidth, billboard ? BillboardMobHeight : 1.15f, halfWidth);
    if (a.hurtCooldown <= 0.0f && w.aabbTouchesBlock(mobMinimum, mobMaximum, Block::Cactus)) {
        a.health -= 1.0f;
        a.hurtCooldown = 0.65f;
        a.hurtFlash = 0.22f;
        if (sounds_) {
            const MobSoundType type = mobSoundType(static_cast<std::uint8_t>(a.type));
            if (a.health <= 0.0f) sounds_->playMobDeath(type, a.position);
            else sounds_->playMobHurt(type, a.position);
        }
        if (a.health <= 0.0f) {
            a.deathTimer = 0.65f;
            releaseDrops(a);
            spawnExperience(a.position, 2);
        }
    }
}
void SurvivalWorld::update(float dt, World& w, Player& p, Inventory& i, float day) {
    if (!p.isSpectator()) {
        lastActivePlayerPosition_ = p.position();
        hasActivePlayerPosition_ = true;
    }
    spawnAccumulator_ += dt;
    passiveSpawnCooldown_ = std::max(0.0f, passiveSpawnCooldown_ - dt);
    if (spawnAccumulator_ >= 5.0f) {
        spawnAccumulator_ = std::fmod(spawnAccumulator_, 5.0f);
        for (const StructureMobMarker& marker :
             w.takeActiveStructureMobMarkers(!p.isSpectator()))
            spawnStructureMob({marker.x, marker.y, marker.z}, marker.hostile);
        if (!p.isSpectator())
            spawnNearbyAnimals(w, p.position(), day);
    }
    navigationQueriesLastTick_ = 0;
    const auto aiStart = std::chrono::steady_clock::now();
    const glm::vec3 playerPosition = p.position();
    const glm::vec3 despawnReference = p.isSpectator()
        ? (hasActivePlayerPosition_ ? lastActivePlayerPosition_ : glm::vec3(0.0f))
        : playerPosition;
    auto intervalFor = [&](const glm::vec3& position, float nearDistance) {
        const glm::vec2 delta(position.x - playerPosition.x, position.z - playerPosition.z);
        const float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared < nearDistance * nearDistance)
            return 0.0f;
        return distanceSquared < 80.0f * 80.0f ? 0.10f : 0.20f;
    };
    for (auto& a : animals_) {
        if (!w.simulationActiveAt(a.position.x, a.position.z))
            continue;
        a.simulationAccumulator += dt;
        const float interval = intervalFor(a.position, 24.0f);
        if (a.simulationAccumulator >= interval) {
            updateAnimal(a, a.simulationAccumulator, w, p, i, day);
            a.simulationAccumulator = 0.0f;
        }
    }
    updateArrows(dt, w, p);
    aiMilliseconds_ = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - aiStart).count();
    // Natural populations stay local even when the simulation radius is large.
    // Persistent (saved or bred) animals are never removed by this rule.
    const float despawnDistance = std::clamp(
        static_cast<float>(w.simulationDistance() * CHUNK_SIZE + 16), 48.0f, 160.0f);
    animals_.erase(std::remove_if(animals_.begin(),
                                  animals_.end(),
                                  [&](const Animal& animal) {
                                      if (animal.deathTimer < 0)
                                          return true;
                                      if (animal.persistent)
                                          return false;
                                      const glm::vec2 delta(animal.position.x - despawnReference.x,
                                                            animal.position.z - despawnReference.z);
                                      const float limit = isBillboard(animal.type) ||
                                                          animal.type == AnimalType::Pillager
                                                              ? despawnDistance
                                                              : despawnDistance + 48.0f;
                                      return glm::dot(delta, delta) > limit * limit;
                                  }),
                   animals_.end());
    for (auto& d : drops_) {
        if (!w.simulationActiveAt(d.position.x, d.position.z))
            continue;
        d.simulationAccumulator += dt;
        const float interval = intervalFor(d.position, 12.0f);
        if (d.simulationAccumulator < interval)
            continue;
        const float step = d.simulationAccumulator;
        d.simulationAccumulator = 0.0f;
        d.age += step;
        d.velocity.y -= 9.8f * step;
        glm::vec3 n = d.position + d.velocity * step;
        if (w.isSolidAt(static_cast<int>(std::floor(n.x)),
                        static_cast<int>(std::floor(n.y)),
                        static_cast<int>(std::floor(n.z))))
            d.velocity = glm::vec3(0);
        else
            d.position = n;
        if (!p.isSpectator() && d.age >= d.pickupDelay &&
            glm::distance(d.position, p.position() + glm::vec3(0, .8f, 0)) < 1.4f) {
            const int before = d.stack.count;
            int left = i.add(d.stack.item, d.stack.count, d.stack.durability);
            d.stack.count = left;
            if (sounds_ && left < before) sounds_->playItemPickup();
        }
    }
    drops_.erase(std::remove_if(drops_.begin(),
                                drops_.end(),
                                [](const Drop& d) { return d.age > 180 || d.stack.count <= 0; }),
                 drops_.end());
    for (auto& o : experienceOrbs_) {
        if (!w.simulationActiveAt(o.position.x, o.position.z))
            continue;
        o.simulationAccumulator += dt;
        const float interval = intervalFor(o.position, 12.0f);
        if (o.simulationAccumulator < interval)
            continue;
        const float step = o.simulationAccumulator;
        o.simulationAccumulator = 0.0f;
        o.age += step;
        glm::vec3 target = p.position() + glm::vec3(0, .8f, 0), delta = target - o.position;
        float dist = glm::length(delta);
        if (!p.isSpectator() && dist < 7 && dist > .01f)
            o.velocity += glm::normalize(delta) * (10.f * step);
        o.velocity *= std::pow(.45f, step);
        o.position += o.velocity * step;
        if (!p.isSpectator() && dist < .75f) {
            p.addExperience(o.value);
            if (sounds_ && o.value > 0) sounds_->playXpPickup();
            o.value = 0;
        }
    }
    experienceOrbs_.erase(
        std::remove_if(experienceOrbs_.begin(),
                       experienceOrbs_.end(),
                       [](const ExperienceOrb& o) { return o.age > 120 || o.value <= 0; }),
        experienceOrbs_.end());
}

SurvivalWorld::MobDiagnostics SurvivalWorld::diagnostics(
    const glm::vec3& playerPosition) const {
    (void)playerPosition;
    MobDiagnostics result;
    result.passiveCap = PassiveMobCap;
    result.hostileCap = HostileMobCap;
    result.spawnAttempts = spawnAttemptsLastTick_;
    result.spawnSuccesses = spawnSuccessesLastTick_;
    result.navigationQueries = navigationQueriesLastTick_;
    result.aiMilliseconds = aiMilliseconds_;
    result.arrows = static_cast<int>(arrows_.size());
    for (const Animal& animal : animals_) {
        if (animal.deathTimer > 0)
            continue;
        (isBillboard(animal.type) || animal.type == AnimalType::Pillager
            ? result.hostile : result.passive)++;
    }
    return result;
}
std::vector<RenderCuboid> SurvivalWorld::renderCuboids() const {
    std::vector<RenderCuboid> out;
    out.reserve(animals_.size() * 6 + experienceOrbs_.size() + arrows_.size());
    for (const auto& a : animals_) {
        if (isBillboard(a.type))
            continue;
        float baby = a.age < 0 ? .55f : 1,
              death = a.deathTimer > 0 ? std::max(.1f, a.deathTimer / .65f) : 1;
        glm::vec3 c =
            a.type == AnimalType::Cow
                ? glm::vec3(.35f, .2f, .12f)
                : (a.type == AnimalType::Pig
                       ? glm::vec3(.95f, .55f, .6f)
                       : (a.type == AnimalType::Sheep ? glm::vec3(.9f)
                       : a.type == AnimalType::Villager ? glm::vec3(.60f, .44f, .28f)
                       : a.type == AnimalType::Pillager ? glm::vec3(.37f, .42f, .48f)
                       : glm::vec3(.38f)));
        if (a.hurtFlash > 0)
            c = glm::mix(c, glm::vec3(1, .04f, .03f), .8f);
        float scale = baby * death;
        out.push_back(
            {a.position + glm::vec3(0, .55f * scale, 0), glm::vec3(1, .85f, .55f) * scale, c});
        out.push_back({a.position + glm::vec3(a.heading.x * .42f, .75f * scale, a.heading.y * .42f),
                       glm::vec3(.55f) * scale,
                       c * 1.08f});
        for (int j = 0; j < 4; ++j)
            out.push_back(
                {a.position +
                     glm::vec3((j < 2 ? -.3f : .3f) * scale, .18f, (j % 2 ? -.22f : .22f) * scale),
                 glm::vec3(.16f, .48f, .16f) * scale,
                 c * .75f});
    }
    for (const auto& o : experienceOrbs_)
        out.push_back({o.position, glm::vec3(.14f), {.35f, 1, .08f}});
    for (const Arrow& arrow : arrows_)
        out.push_back({arrow.position, glm::vec3(.055f, .48f, .055f),
                       glm::vec3(.34f, .24f, .14f), arrow.velocity});
    return out;
}
std::vector<RenderBillboard> SurvivalWorld::renderBillboards() const {
    std::vector<RenderBillboard> out;
    for (const Animal& a : animals_)
        if (isBillboard(a.type) && a.deathTimer >= 0.0f)
            out.push_back({a.position, static_cast<std::uint8_t>(billboardIndex(a.type)), a.hurtFlash,
                           a.deathTimer > 0.0f ? std::clamp(a.deathTimer / .65f, 0.0f, 1.0f) : 1.0f});
    return out;
}
std::vector<RenderItemSprite> SurvivalWorld::renderItemSprites() const {
    std::vector<RenderItemSprite> out;
    out.reserve(drops_.size());
    for (const auto& d : drops_)
        out.push_back({d.position, d.stack.item, .42f});
    return out;
}
bool SurvivalWorld::save(const std::string& p, std::uint32_t seed) const {
    return SaveFile::write(p, std::ios::binary, [&](std::ofstream& f) {
    f.write(MobMagic, 8);
    wr(f, seed);
    std::uint32_t n = static_cast<std::uint32_t>(std::count_if(
        animals_.begin(), animals_.end(), [](const Animal& a) { return a.deathTimer <= 0; }));
    wr(f, n);
    for (const auto& a : animals_)
        if (a.deathTimer <= 0) {
            std::uint8_t type = 0;
            switch (a.type) {
            case AnimalType::Cow: type = 0; break;
            case AnimalType::Pig: type = 1; break;
            case AnimalType::Sheep: type = 2; break;
            case AnimalType::Villager: type = 4; break;
            case AnimalType::Pillager: type = 5; break;
            default:
                if (isBillboard(a.type)) type = static_cast<std::uint8_t>(a.type);
                break;
            }
            wr(f, type);
            wr(f, a.position);
            wr(f, a.health);
            wr(f, a.age);
        }
    const std::uint32_t markerCount =
        static_cast<std::uint32_t>(spawnedStructureMarkers_.size());
    wr(f, markerCount);
    for (std::uint64_t id : spawnedStructureMarkers_)
        wr(f, id);
    // Filename identities survive changes to sprite ordering between releases.
    f.write("BID1", 4);
    wr(f, n);
    for (const Animal& a : animals_) if (a.deathTimer <= 0) {
        const std::string name = isBillboard(a.type)
            ? billboardMobName(billboardIndex(a.type)) : std::string{};
        const auto length = static_cast<std::uint8_t>(name.size());
        wr(f, length);
        f.write(name.data(), length);
    }
    return !!f;
    });
}
bool SurvivalWorld::load(const std::string& p, std::uint32_t seed) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return false;
    char m[8]{};
    std::uint32_t stored = 0, n = 0;
    f.read(m, 8);
    if (std::memcmp(m, MobMagic, 8) || !rd(f, stored) || stored != seed || !rd(f, n) || n > 5000)
        return false;
    std::vector<Animal> loaded;
    std::vector<std::uint32_t> recordIndices;
    decltype(spawnedChunks_) loadedSpawnedChunks;
    decltype(spawnedStructureMarkers_) loadedMarkers;
    for (std::uint32_t k = 0; k < n; ++k) {
        std::uint8_t type;
        Animal a;
        if (!rd(f, type) || !rd(f, a.position) || !rd(f, a.health) || !rd(f, a.age))
            return false;
        switch (type) {
        case 0: a.type = AnimalType::Cow; break;
        case 1: a.type = AnimalType::Pig; break;
        case 2: a.type = AnimalType::Sheep; break;
        case 3: continue; // Retired Wolf from an older save.
        case 4: a.type = AnimalType::Villager; break;
        case 5: a.type = AnimalType::Pillager; break;
        default:
            if (type < 6 || type >= 6 + BillboardMobCount) return false;
            a.type = static_cast<AnimalType>(type);
            break;
        }
        a.health = std::clamp(a.health, .1f, 20.f);
        a.home = a.position;
        a.persistent = true;
        recordIndices.push_back(k);
        loaded.push_back(a);
        if (!isBillboard(a.type) && a.type != AnimalType::Pillager) {
            loadedSpawnedChunks.insert(
                chunkKey(static_cast<int>(std::floor(a.position.x / CHUNK_SIZE)),
                         static_cast<int>(std::floor(a.position.z / CHUNK_SIZE))));
        }
    }
    // The marker section was appended after the original mob records. Old
    // MOB1 files end here and remain readable without migration.
    if (f.peek() != std::char_traits<char>::eof()) {
        std::uint32_t markerCount = 0;
        if (!rd(f, markerCount) || markerCount > 100000U)
            return false;
        for (std::uint32_t index = 0; index < markerCount; ++index) {
            std::uint64_t id = 0;
            if (!rd(f, id)) return false;
            loadedMarkers.insert(id);
        }
        if (f.peek() != std::char_traits<char>::eof()) {
            char identityMagic[4]{};
            std::uint32_t identityCount = 0;
            f.read(identityMagic, 4);
            const bool legacyVariants = std::memcmp(identityMagic, "VAR1", 4) == 0;
            const bool namedTypes = std::memcmp(identityMagic, "BID1", 4) == 0;
            if ((!legacyVariants && !namedTypes) || !rd(f, identityCount) ||
                identityCount != n)
                return false;
            std::size_t loadedIndex = 0;
            for (std::uint32_t record = 0; record < n; ++record) {
                std::uint8_t value = 0;
                if (!rd(f, value)) return false;
                std::string name;
                if (namedTypes) {
                    name.resize(value);
                    f.read(name.data(), value);
                    if (!f) return false;
                }
                if (loadedIndex < recordIndices.size() && recordIndices[loadedIndex] == record) {
                    Animal& animal = loaded[loadedIndex++];
                    if (legacyVariants && isBillboard(animal.type))
                        animal.type = billboardType(value < BillboardMobCount ? value : 0);
                    if (namedTypes && !name.empty()) {
                        for (std::size_t sprite = 0; sprite < BillboardMobCount; ++sprite)
                            if (name == billboardMobName(sprite)) {
                                animal.type = billboardType(sprite);
                                break;
                            }
                    }
                }
            }
        }
    }
    animals_ = std::move(loaded);
    arrows_.clear();
    explosionEffects_.clear();
    spawnedChunks_ = std::move(loadedSpawnedChunks);
    spawnedStructureMarkers_ = std::move(loadedMarkers);
    return true;
}
bool SurvivalWorld::runCombatSelfTest(World& sourceWorld, Player& p, Inventory& i, std::string& report) {
    (void)sourceWorld;
    (void)p;
    (void)i;
    Inventory combatInventory;
    combatInventory.clear();
    World w(seed_);
    w.generate(4, glm::vec3(.5f, 80.0f, .5f));
    w.setSimulationDistance(4);
    Player testPlayer(w.findSafeSpawnNear(0, 0));
    auto oldAnimals = animals_;
    auto oldDrops = drops_;
    auto oldOrbs = experienceOrbs_;
    auto oldArrows = arrows_;
    auto oldEffects = explosionEffects_;
    auto oldSpawned = spawnedChunks_;
    animals_.clear();
    drops_.clear();
    experienceOrbs_.clear();
    bool ok = true;
    std::string firstFailure;
    float hitDistance = 0.0f;
    const glm::vec3 boxMin(-BillboardMobHalfWidth, 0.0f, -BillboardMobHalfWidth);
    const glm::vec3 boxMax(BillboardMobHalfWidth, BillboardMobHeight, BillboardMobHalfWidth);
    ok &= rayBox({0, BillboardMobHeight - .01f, -2}, {0, 0, 1},
                 boxMin, boxMax, 5, hitDistance) &&
          !rayBox({0, BillboardMobHeight + .01f, -2}, {0, 0, 1},
                  boxMin, boxMax, 5, hitDistance) &&
          !rayBox({BillboardMobHalfWidth + .01f, .75f, -2}, {0, 0, 1},
                  boxMin, boxMax, 5, hitDistance);
    if (!ok) firstFailure = "billboard hitbox dimensions";
    std::array<AnimalType, 4> types{
        AnimalType::Cow, AnimalType::Pig, AnimalType::Sheep, billboardType(0)};
    std::array<Item, 4> expected{Item::RawBeef, Item::RawPork, Item::Wool, Item::None};
    glm::vec3 origin = testPlayer.cameraPosition(), dir = testPlayer.lookDirection();
    for (int k = 0; k < 4; ++k) {
        Animal a;
        a.type = types[k];
        a.position = origin + dir * 3.f - glm::vec3(0,
            isBillboard(a.type) ? BillboardMobHeight * .5f : .55f, 0);
        a.health = isBillboard(types[k])
            ? billboardMobDefinitions()[billboardIndex(types[k])].health
            : (types[k] == AnimalType::Cow ? 10.f : 8.f);
        animals_.push_back(a);
        MobTarget target = raycastMob(origin, dir, 5, w, 5);
        if (!target.valid() && firstFailure.empty())
            firstFailure = "ray target variant " + std::to_string(k);
        ok &= target.valid();
        while (target.valid() && animals_[target.index].deathTimer <= 0) {
            animals_[target.index].hurtCooldown = 0;
            attackMob(target.index, Item::IronAxe, false, testPlayer.position());
            if (k == 3 && animals_[target.index].deathTimer <= 0)
                ok &= animals_[target.index].hurtFlash > 0.0f;
        }
        if (expected[k] != Item::None) {
            const bool dropped = std::any_of(drops_.begin(), drops_.end(), [&](const Drop& d) {
                return d.stack.item == expected[k];
            });
            if (!dropped && firstFailure.empty()) firstFailure = "drop variant " + std::to_string(k);
            ok &= dropped;
            int before = combatInventory.count(expected[k]);
            for (auto& drop : drops_)
                drop.position = testPlayer.position() + glm::vec3(0, .8f, 0);
            update(0, w, testPlayer, combatInventory, 1);
            const bool picked = combatInventory.count(expected[k]) > before;
            if (!picked && firstFailure.empty()) firstFailure = "pickup variant " + std::to_string(k);
            ok &= picked;
        } else
            ok &= drops_.empty();
        if (!ok && firstFailure.empty()) firstFailure = "target, death, or drops variant " + std::to_string(k);
        animals_.clear();
        drops_.clear();
        experienceOrbs_.clear();
        spawnedChunks_.clear();
    }
    Animal blocked;
    blocked.position = origin + dir * 3.f - glm::vec3(0, .55f, 0);
    animals_.push_back(blocked);
    ok &= !raycastMob(origin, dir, 5, w, 1.5f).valid();
    if (!ok && firstFailure.empty()) firstFailure = "ray occlusion";
    animals_.clear();
    Animal hunter;
    hunter.type = billboardType(1); // KitaIkuyo is the melee hunter.
    hunter.position = testPlayer.position() + glm::vec3(1.f, 0, 0);
    hunter.health = 12;
    animals_.push_back(hunter);
    float healthBefore = testPlayer.health();
    updateAnimal(animals_.front(), .02f, w, testPlayer, combatInventory, 0);
    ok &= std::abs((healthBefore - testPlayer.health()) - 2.0f) < .001f;
    const float healthAfterHit = testPlayer.health();
    updateAnimal(animals_.front(), .02f, w, testPlayer, combatInventory, 0);
    ok &= testPlayer.health() == healthAfterHit;
    testPlayer.setCreativeMode(true);
    const float hunterCreativeHealth = testPlayer.health();
    animals_.front().attackCooldown = 0.0f;
    updateAnimal(animals_.front(), .02f, w, testPlayer, combatInventory, 0);
    ok &= testPlayer.health() == hunterCreativeHealth;
    testPlayer.setCreativeMode(false);
    testPlayer.setSpectatorMode(true);
    const float spectatorHealth = testPlayer.health();
    updateAnimal(animals_.front(), .02f, w, testPlayer, combatInventory, 0);
    ok &= testPlayer.health() == spectatorHealth;
    if (!ok && firstFailure.empty()) firstFailure = "hostile melee";
    animals_.clear();
    Player pillagerVictim(w.findSafeSpawnNear(0, 0));
    Animal pillager;
    pillager.type = AnimalType::Pillager;
    pillager.position = pillagerVictim.position() + glm::vec3(1.0f, 0.0f, 0.0f);
    pillager.home = pillager.position;
    pillager.health = 14.0f;
    animals_.push_back(pillager);
    const float pillagerVictimHealth = pillagerVictim.health();
    updateAnimal(animals_.front(), .02f, w, pillagerVictim, combatInventory, 1.0f);
    ok &= pillagerVictim.health() < pillagerVictimHealth;
    pillagerVictim.setCreativeMode(true);
    const float creativeHealth = pillagerVictim.health();
    animals_.front().attackCooldown = 0.0f;
    updateAnimal(animals_.front(), .02f, w, pillagerVictim, combatInventory, 1.0f);
    ok &= pillagerVictim.health() == creativeHealth;
    if (!ok && firstFailure.empty()) firstFailure = "pillager or creative";

    // Exercise each role against a clear high-altitude test lane. The terrain
    // checks below remain local to this temporary smoke-test world.
    const glm::vec3 lane(0.5f, 248.0f, 0.5f);
    for (std::size_t index = 0; index < BillboardMobCount; ++index) {
        const auto& definition = billboardMobDefinitions()[index];
        const BillboardBehavior expectedBehavior = index == 0
            ? BillboardBehavior::ExplosiveChase : index == 1
            ? BillboardBehavior::MeleeChase : index == 2
            ? BillboardBehavior::RangedAttack : BillboardBehavior::WallClimbLunge;
        ok &= definition.behavior == expectedBehavior;
    }
    if (!ok && firstFailure.empty()) firstFailure = "billboard role definitions";
    animals_.clear();
    Player distantVictim(lane);
    Animal longHunter;
    longHunter.type = billboardType(1);
    longHunter.position = lane + glm::vec3(30, 0, 0);
    animals_.push_back(longHunter);
    updateAnimal(animals_.front(), .02f, w, distantVictim, combatInventory, 0);
    ok &= animals_.front().memoryTimer > 0.0f;
    if (!ok && firstFailure.empty()) firstFailure = "35-block hunter detection";
    animals_.clear();
    arrows_.clear();
    Player blastVictim(lane);
    Animal exploder;
    exploder.type = billboardType(0);
    exploder.position = lane + glm::vec3(2, 0, 0);
    exploder.thinkTimer = 4.0f;
    animals_.push_back(exploder);
    const float initialHealth = blastVictim.health();
    updateAnimal(animals_.front(), .8f, w, blastVictim, combatInventory, 0);
    ok &= blastVictim.health() == initialHealth && animals_.front().fuseTimer > .7f;
    // Knockback/retreat cancels an unfinished fuse.
    blastVictim.teleport(lane + glm::vec3(12, 0, 0));
    updateAnimal(animals_.front(), .5f, w, blastVictim, combatInventory, 0);
    ok &= animals_.front().fuseTimer == 0.0f;
    blastVictim.teleport(lane);
    animals_.front().position = lane + glm::vec3(2, 0, 0);
    const glm::ivec3 softBlock(4, 248, 0);
    w.setBlock(softBlock.x, softBlock.y, softBlock.z, Block::Dirt);
    updateAnimal(animals_.front(), 1.6f, w, blastVictim, combatInventory, 0);
    ok &= animals_.front().deathTimer < 0 && blastVictim.health() < initialHealth &&
          !explosionEffects_.empty() && w.getBlock(softBlock.x, softBlock.y, softBlock.z) == Block::Air;
    if (!ok && firstFailure.empty()) firstFailure = "explosive fuse, damage, or terrain";
    Player creativeVictim(lane);
    creativeVictim.setCreativeMode(true);
    detonate(lane + glm::vec3(2, .75f, 0), w, creativeVictim);
    ok &= creativeVictim.health() == 20.0f;
    Player spectatorVictim(lane);
    spectatorVictim.setSpectatorMode(true);
    detonate(lane + glm::vec3(2, .75f, 0), w, spectatorVictim);
    ok &= spectatorVictim.health() == 20.0f;
    if (!ok && firstFailure.empty()) firstFailure = "explosion creative or spectator immunity";
    animals_.clear();
    explosionEffects_.clear();
    const glm::vec3 shieldOrigin(10.5f, 248.5f, .5f);
    const glm::vec3 shieldTarget(12.5f, 248.5f, .5f);
    w.setBlock(11, 248, 0, Block::Stone);
    ok &= explosionExposure(w, shieldOrigin, shieldTarget) < .5f;
    w.setBlock(11, 248, 0, Block::Air);
    if (!ok && firstFailure.empty()) firstFailure = "explosion obstruction";

    Player rangedVictim(lane);
    Animal archer;
    archer.type = billboardType(2);
    archer.position = lane + glm::vec3(0, 0, 8);
    archer.thinkTimer = 4.0f;
    animals_.push_back(archer);
    updateAnimal(animals_.front(), .02f, w, rangedVictim, combatInventory, 0);
    const bool shotSpawned = arrows_.size() == 1 && rangedVictim.health() == 20.0f;
    const auto projectileModels = renderCuboids();
    ok &= shotSpawned && !projectileModels.empty() &&
          glm::length(projectileModels.back().direction) > 1.0f;
    updateArrows(.6f, w, rangedVictim);
    const bool shotHit = rangedVictim.health() < 20.0f && arrows_.empty();
    ok &= shotHit;
    arrows_.clear();
    Player shieldedVictim(lane);
    for (int y = 248; y <= 250; ++y)
        w.setBlock(0, y, 4, Block::Stone);
    arrows_.push_back({lane + glm::vec3(0, 1.1f, 8), glm::vec3(0, 0, -17)});
    updateArrows(.6f, w, shieldedVictim);
    const bool shotBlocked = shieldedVictim.health() == 20.0f && arrows_.empty();
    ok &= shotBlocked;
    for (int y = 248; y <= 250; ++y)
        w.setBlock(0, y, 4, Block::Air);
    Player wallBehindVictim(lane);
    w.setBlock(0, 249, -1, Block::Stone);
    arrows_.push_back({lane + glm::vec3(0, 1.1f, .8f),
                       glm::vec3(0, 0, -100.0f)});
    updateArrows(.025f, w, wallBehindVictim);
    const bool playerBeforeWall = wallBehindVictim.health() == 17.0f && arrows_.empty();
    w.setBlock(0, 249, -1, Block::Air);
    ok &= playerBeforeWall;
    if (!ok && firstFailure.empty()) firstFailure = "ranged projectile (spawn=" +
        std::to_string(shotSpawned) + ", hit=" + std::to_string(shotHit) +
        ", blocked=" + std::to_string(shotBlocked) +
        ", player-first=" + std::to_string(playerBeforeWall) + ")";
    const auto queueSureHit = [&](int count) {
        for (int shot = 0; shot < count; ++shot)
            arrows_.push_back({lane + glm::vec3(0, 1.1f, 2.0f),
                               glm::vec3(0, 0, -17.0f), 0.0f, true,
                               static_cast<std::uint8_t>(billboardType(2))});
    };
    for (int count : {1, 2, 3, 5, 10}) {
        arrows_.clear();
        Player volleyVictim(lane);
        queueSureHit(count);
        updateArrows(.15f, w, volleyVictim);
        const float expectedHealth = std::max(0.0f, 20.0f - count * 3.0f);
        const bool volleyPassed = std::abs(volleyVictim.health() - expectedHealth) < .001f &&
                                  volleyVictim.isDead() == (expectedHealth == 0.0f) &&
                                  (count > 5 || arrows_.empty());
        if (!volleyPassed && firstFailure.empty())
            firstFailure = "projectile volley " + std::to_string(count) +
                " dealt " + std::to_string(20.0f - volleyVictim.health());
        ok &= volleyPassed;
        if (count <= 5) {
            const float afterHit = volleyVictim.health();
            updateArrows(.15f, w, volleyVictim);
            ok &= arrows_.empty() && volleyVictim.health() == afterHit;
        }
    }
    arrows_.clear();
    Player massVolleyVictim(lane);
    queueSureHit(64); // The active projectile safety limit, with lethal damage.
    updateArrows(.15f, w, massVolleyVictim);
    ok &= massVolleyVictim.isDead() && massVolleyVictim.health() == 0.0f &&
          arrows_.size() <= 64;
    if (!ok && firstFailure.empty()) firstFailure = "mass projectile volley";
    arrows_.clear();
    Player twoArchersVictim(lane);
    animals_.clear();
    for (int offset : {7, 8}) {
        Animal source;
        source.type = billboardType(2);
        source.position = lane + glm::vec3(0, 0, static_cast<float>(offset));
        source.thinkTimer = 4.0f;
        animals_.push_back(source);
        updateAnimal(animals_.back(), .02f, w, twoArchersVictim, combatInventory, 0);
    }
    const bool twoSourcesFired = arrows_.size() == 2;
    for (Arrow& shot : arrows_) {
        shot.position = lane + glm::vec3(0, 1.1f, 2.0f);
        shot.velocity = {0, 0, -17.0f};
    }
    updateArrows(.15f, w, twoArchersVictim);
    ok &= twoSourcesFired && twoArchersVictim.health() == 14.0f;
    if (!ok && firstFailure.empty()) firstFailure = "arrows from two NijikaIjichi";
    animals_.clear();
    arrows_.clear();
    Player staggeredVictim(lane);
    for (float interval : {.005f, .1f}) {
        arrows_.push_back({lane + glm::vec3(0, 1.1f, .35f),
                           glm::vec3(0, 0, -17.0f)});
        updateArrows(interval, w, staggeredVictim);
    }
    ok &= std::abs(staggeredVictim.health() - 14.0f) < .001f;
    if (!ok && firstFailure.empty()) firstFailure = "staggered projectile hits";
    arrows_.clear();
    Player immuneVictim(lane);
    immuneVictim.setCreativeMode(true);
    queueSureHit(3);
    updateArrows(.15f, w, immuneVictim);
    ok &= immuneVictim.health() == 20.0f;
    arrows_.clear();
    immuneVictim.setCreativeMode(false);
    immuneVictim.setSpectatorMode(true);
    queueSureHit(3);
    updateArrows(.15f, w, immuneVictim);
    ok &= immuneVictim.health() == 20.0f;
    arrows_.clear();
    Player cooldownVictim(lane);
    cooldownVictim.damage(2.0f);
    cooldownVictim.damage(2.0f); // Repeated contact remains protected.
    cooldownVictim.damage(3.0f, PlayerDamageSource::Projectile);
    cooldownVictim.damage(2.0f); // Projectile hits do not remove contact protection.
    ok &= cooldownVictim.health() == 15.0f;
    if (!ok && firstFailure.empty()) firstFailure = "projectile immunity or contact cooldown";
    w.setBlock(0, 247, 0, Block::Stone);
    Player fallingVictim(lane + glm::vec3(0, 6.0f, 0));
    float fallDamage = 0.0f;
    for (int tick = 0; tick < 80 && !fallingVictim.isGrounded(); ++tick) {
        fallingVictim.update(.05f, PlayerInput{}, w);
        fallDamage = std::max(fallDamage, fallingVictim.lastFallDamage());
    }
    ok &= fallDamage > 0.0f && fallingVictim.health() < 20.0f;
    if (!ok && firstFailure.empty()) firstFailure = "fall damage after projectile hits";
    w.setBlock(0, 247, 0, Block::Air);
    animals_.clear();
    Player lungeVictim(lane);
    Animal climber;
    climber.type = billboardType(3);
    climber.position = lane + glm::vec3(4, 0, 0);
    climber.grounded = true;
    climber.thinkTimer = 4.0f;
    animals_.push_back(climber);
    updateAnimal(animals_.front(), .02f, w, lungeVictim, combatInventory, 0);
    ok &= animals_.front().lungeCooldown > 2.0f && animals_.front().velocity.y > 0.0f;
    const float firstLungeCooldown = animals_.front().lungeCooldown;
    updateAnimal(animals_.front(), .02f, w, lungeVictim, combatInventory, 0);
    ok &= animals_.front().lungeCooldown < firstLungeCooldown;
    if (!ok && firstFailure.empty()) firstFailure = "wall climber lunge";
    animals_.clear();
    Player climbTarget(lane + glm::vec3(4.0f, 0, 0));
    climber = Animal{};
    climber.type = billboardType(3);
    climber.position = lane + glm::vec3(1.12f, 0, 0);
    climber.grounded = true;
    climber.memoryTimer = 3.0f;
    climber.rememberedTarget = climbTarget.position();
    w.setBlock(1, 247, 0, Block::Stone);
    for (int y = 248; y <= 250; ++y)
        w.setBlock(2, y, 0, Block::Stone);
    animals_.push_back(climber);
    updateAnimal(animals_.front(), .1f, w, climbTarget, combatInventory, 0);
    const bool climbed = animals_.front().position.y > lane.y + .05f;
    for (int y = 248; y <= 250; ++y)
        w.setBlock(2, y, 0, Block::Air);
    w.setBlock(1, 247, 0, Block::Air);
    animals_.front().lungeCooldown = 2.0f;
    const float climbVelocity = animals_.front().velocity.y;
    updateAnimal(animals_.front(), .1f, w, climbTarget, combatInventory, 0);
    ok &= climbed && animals_.front().velocity.y < climbVelocity;
    if (!ok && firstFailure.empty()) firstFailure = "wall contact climbing (climbed=" +
        std::to_string(climbed) + ", velocity=" +
        std::to_string(animals_.front().velocity.y) + ", prior=" +
        std::to_string(climbVelocity) + ")";
    animals_.clear();

    // A walking path over a full-block rise must become an actual jump rather
    // than a navigation heading that is canceled by collision each tick.
    for (int x = 0; x <= 22; ++x)
        for (int z = -2; z <= 2; ++z)
            w.setBlock(x, 247, z, Block::Stone);
    for (int x = 3; x <= 5; ++x)
        for (int z = -2; z <= 2; ++z)
            w.setBlock(x, 248, z, Block::Stone);
    Player stepTarget({20.5f, 248.01f, .5f});
    for (std::size_t role = 0; role < BillboardMobCount; ++role) {
        Animal walker;
        walker.type = billboardType(role);
        walker.position = {1.5f, 248.01f, .5f};
        walker.heading = {1, 0};
        walker.grounded = true;
        walker.memoryTimer = 12.0f;
        walker.rememberedTarget = stepTarget.position();
        animals_.push_back(walker);
        bool reachedStep = false;
        for (int tick = 0; tick < 150 && !reachedStep; ++tick) {
            updateAnimal(animals_.back(), .04f, w, stepTarget, combatInventory, 0);
            reachedStep = animals_.back().position.x > 3.4f &&
                          animals_.back().position.y >= 248.95f;
        }
        if (!reachedStep && firstFailure.empty())
            firstFailure = "one-block step for role " + std::to_string(role) +
                " at " + std::to_string(animals_.back().position.x) + "," +
                std::to_string(animals_.back().position.y);
        ok &= reachedStep;
        animals_.clear();
    }
    for (int z = -2; z <= 2; ++z) {
        w.setBlock(4, 249, z, Block::Stone);
        w.setBlock(5, 249, z, Block::Stone);
        w.setBlock(5, 250, z, Block::Stone);
    }
    for (std::size_t role = 0; role < BillboardMobCount; ++role) {
        Animal walker;
        walker.type = billboardType(role);
        walker.position = {1.5f, 248.01f, .5f};
        walker.heading = {1, 0};
        walker.grounded = true;
        walker.memoryTimer = 16.0f;
        walker.rememberedTarget = stepTarget.position();
        animals_.push_back(walker);
        bool climbedStairs = false;
        for (int tick = 0; tick < 250 && !climbedStairs; ++tick) {
            updateAnimal(animals_.back(), .04f, w, stepTarget, combatInventory, 0);
            if (animals_.back().position.y < 247.0f) {
                if (firstFailure.empty())
                    firstFailure = "stair fall for role " + std::to_string(role) +
                        " at " + std::to_string(animals_.back().position.x) + "," +
                        std::to_string(animals_.back().position.y) + "," +
                        std::to_string(animals_.back().position.z) +
                        " tick " + std::to_string(tick);
                break;
            }
            climbedStairs = animals_.back().position.x > 5.35f &&
                            animals_.back().position.y >= 250.95f;
        }
        if (!climbedStairs && firstFailure.empty())
            firstFailure = "stair climb for role " + std::to_string(role) +
                " at " + std::to_string(animals_.back().position.x) + "," +
                std::to_string(animals_.back().position.y) + "," +
                std::to_string(animals_.back().position.z);
        ok &= climbedStairs;
        animals_.clear();
    }
    for (int z = -2; z <= 2; ++z) {
        w.setBlock(4, 249, z, Block::Air);
        w.setBlock(5, 249, z, Block::Air);
        w.setBlock(5, 250, z, Block::Air);
    }
    // A roof one cell above the ledge lacks the 1.75-block headroom needed
    // for either a jump or a standable destination.
    for (int x = 1; x <= 5; ++x)
        for (int z = -1; z <= 1; ++z)
            w.setBlock(x, 250, z, Block::Stone);
    float blockedSurface = 0.0f;
    const bool lowRoofBlocked = !billboardFooting(
        w, {3.5f, .5f}, 248.01f, .08f, 1.02f, blockedSurface);
    for (int x = 1; x <= 5; ++x)
        for (int z = -1; z <= 1; ++z)
            w.setBlock(x, 250, z, Block::Air);
    for (int z = -2; z <= 2; ++z)
        w.setBlock(3, 249, z, Block::Stone);
    const bool tallWallBlocked = !billboardFooting(
        w, {3.5f, .5f}, 248.01f, .08f, 1.02f, blockedSurface);
    for (int z = -2; z <= 2; ++z)
        w.setBlock(3, 249, z, Block::Air);
    w.setBlock(3, 248, 0, Block::WoodenSlab);
    const bool slabPassable = billboardFooting(
        w, {3.5f, .5f}, 248.01f, .08f, 1.02f, blockedSurface) &&
        std::abs(blockedSurface - 248.5f) < .02f;
    w.setBlock(3, 248, 0, Block::Snow);
    const bool snowPassable = !billboardTouchesSolid(w, {3.5f, 248.01f, .5f}) &&
        billboardFooting(w, {3.5f, .5f}, 248.01f, -.05f, .05f, blockedSurface) &&
        std::abs(blockedSurface - 248.0f) < .02f;
    w.setBlock(3, 248, 0, Block::Stone);
    if (!(lowRoofBlocked && tallWallBlocked && slabPassable && snowPassable) &&
        firstFailure.empty()) firstFailure = "step clearance or partial-block navigation (roof " +
            std::to_string(lowRoofBlocked) + ", wall " + std::to_string(tallWallBlocked) +
            ", slab " + std::to_string(slabPassable) + ", snow " +
            std::to_string(snowPassable) + ")";
    ok &= lowRoofBlocked && tallWallBlocked && slabPassable && snowPassable;

    animals_.clear();
    drops_.clear();
    spawnedChunks_.clear();
    Inventory pickupInventory;
    pickupInventory.clear();
    Player pickupPlayer(testPlayer.position());
    spawnDrop(pickupPlayer.position() + glm::vec3(0.0f, 0.8f, 0.0f),
              Item::DiamondPickaxe,
              1,
              321,
              glm::vec3(0.0f),
              1.0f);
    update(0.2f, w, pickupPlayer, pickupInventory, 1.0f);
    ok &= pickupInventory.count(Item::DiamondPickaxe) == 0 && drops_.size() == 1 &&
          drops_.front().stack.durability == 321;
    if (!drops_.empty()) {
        drops_.front().age = 1.1f;
        drops_.front().position = pickupPlayer.position() + glm::vec3(0.0f, 0.8f, 0.0f);
        drops_.front().velocity = glm::vec3(0.0f);
    }
    update(0.0f, w, pickupPlayer, pickupInventory, 1.0f);
    ok &= pickupInventory.count(Item::DiamondPickaxe) == 1 &&
          pickupInventory.selectedStack().durability == 321;
    if (!ok && firstFailure.empty()) firstFailure = "item pickup";
    animals_.clear();
    Animal variant;
    variant.position = testPlayer.position() + glm::vec3(2.0f, 0.0f, 0.0f);
    for (std::size_t sprite = 0; sprite < BillboardMobCount; ++sprite) {
        variant.type = billboardType(sprite);
        animals_.push_back(variant);
    }
    const std::string variantPath = "voxel_billboard_variant_smoke.vxm";
    SurvivalWorld reloaded(seed_);
    ok &= save(variantPath, seed_) && reloaded.load(variantPath, seed_);
    const auto billboards = reloaded.renderBillboards();
    ok &= billboards.size() == BillboardMobCount;
    for (std::size_t sprite = 0; sprite < BillboardMobCount; ++sprite)
        ok &= reloaded.mobName(static_cast<int>(sprite)) == billboardMobName(sprite) &&
              reloaded.animals_[sprite].type == billboardType(sprite) &&
              billboards[sprite].variant == sprite;
    if (!ok && firstFailure.empty()) firstFailure = "named mob identity save/load";
    std::filesystem::remove(variantPath);
    const std::string previousVersionPath = "voxel_previous_billboard_smoke.vxm";
    ok &= SaveFile::write(previousVersionPath, std::ios::binary, [&](std::ofstream& output) {
        output.write(MobMagic, 8);
        const std::uint32_t count = 1, markers = 0;
        const std::uint8_t oldType = 6, oldVariant = 2;
        return wr(output, seed_) && wr(output, count) && wr(output, oldType) &&
               wr(output, variant.position) && wr(output, variant.health) &&
               wr(output, variant.age) && wr(output, markers) &&
               (output.write("VAR1", 4), static_cast<bool>(output)) &&
               wr(output, count) && wr(output, oldVariant);
    });
    SurvivalWorld previousVersion(seed_);
    ok &= previousVersion.load(previousVersionPath, seed_) &&
          previousVersion.mobName(0) == billboardMobName(2);
    if (!ok && firstFailure.empty()) firstFailure = "previous billboard save migration";
    std::filesystem::remove(previousVersionPath);
    const std::string legacyPath = "voxel_legacy_wolf_smoke.vxm";
    ok &= SaveFile::write(legacyPath, std::ios::binary, [&](std::ofstream& output) {
        output.write(MobMagic, 8);
        const std::uint32_t count = 1;
        const std::uint8_t retiredWolfId = 3;
        const float health = 12.0f, age = 0.0f;
        const std::uint32_t markers = 0;
        return wr(output, seed_) && wr(output, count) && wr(output, retiredWolfId) &&
               wr(output, variant.position) && wr(output, health) && wr(output, age) &&
               wr(output, markers);
    });
    SurvivalWorld legacy(seed_);
    ok &= legacy.load(legacyPath, seed_) && legacy.renderBillboards().empty() &&
          legacy.diagnostics(testPlayer.position()).passive == 0 &&
          legacy.diagnostics(testPlayer.position()).hostile == 0;
    if (!ok && firstFailure.empty()) firstFailure = "legacy wolf load";
    std::filesystem::remove(legacyPath);
    SurvivalWorld spawnProbe(seed_);
    const auto spawnWaitStart = std::chrono::steady_clock::now();
    while (w.loadedChunkCount() < 25 &&
           std::chrono::steady_clock::now() - spawnWaitStart < std::chrono::seconds(5)) {
        w.updateStreaming(testPlayer.position(), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    for (int attempt = 0; attempt < 20; ++attempt)
        spawnProbe.spawnNearbyAnimals(w, testPlayer.position(), 1.0f);
    ok &= spawnProbe.diagnostics(testPlayer.position()).hostile == 0 &&
          spawnProbe.spawnAttemptsLastTick_ == NaturalSpawnCandidates;
    if (!ok && firstFailure.empty()) firstFailure = "daytime hostile spawn";
    spawnProbe.animals_.clear();
    std::uint8_t variantMask = 0;
    bool sawNightSpawn = false;
    for (int attempt = 0; attempt < 100 && variantMask != 15; ++attempt) {
        spawnProbe.spawnNearbyAnimals(w, testPlayer.position(), 0.0f);
        for (const Animal& spawned : spawnProbe.animals_)
            if (isBillboard(spawned.type)) {
                sawNightSpawn = true;
                variantMask |= static_cast<std::uint8_t>(1U << billboardIndex(spawned.type));
            }
        if (spawnProbe.animals_.size() >= HostileMobCap)
            spawnProbe.animals_.clear();
    }
    const bool boundedNightBatch = spawnProbe.spawnAttemptsLastTick_ ==
        NaturalSpawnCandidates + NightHostileBonusCandidates;
    std::mt19937 rateRandom(0x2600U);
    std::uniform_real_distribution<float> rateRoll(0.0f, 1.0f);
    int oldHostileAttempts = 0, newHostileAttempts = 0;
    for (int batch = 0; batch < 4096; ++batch)
        for (int candidate = 0;
             candidate < NaturalSpawnCandidates + NightHostileBonusCandidates; ++candidate) {
            const bool hostile = rateRoll(rateRandom) < HostileCandidateChance;
            if (candidate < NaturalSpawnCandidates)
                oldHostileAttempts += static_cast<int>(hostile);
            newHostileAttempts += static_cast<int>(hostile);
        }
    const float rateRatio = static_cast<float>(newHostileAttempts) /
                            static_cast<float>(oldHostileAttempts);
    ok &= sawNightSpawn && variantMask == 15 && boundedNightBatch &&
          rateRatio > 1.48f && rateRatio < 1.52f;
    if (!ok && firstFailure.empty()) firstFailure = "night spawn or sprite distribution (spawn=" +
        std::to_string(sawNightSpawn) + ", mask=" + std::to_string(variantMask) +
        ", rate=" + std::to_string(rateRatio) +
        ", x=" + std::to_string(testPlayer.position().x) +
        ", z=" + std::to_string(testPlayer.position().z) +
        ", chunks=" + std::to_string(w.loadedChunkCount()) + ")";
    animals_ = std::move(oldAnimals);
    drops_ = std::move(oldDrops);
    experienceOrbs_ = std::move(oldOrbs);
    arrows_ = std::move(oldArrows);
    explosionEffects_ = std::move(oldEffects);
    spawnedChunks_ = std::move(oldSpawned);
    report = ok ? "mob combat, 1.75-block hitbox, one-block steps/stairs and headroom, night-only spawning (hostile candidate rate " +
                  std::to_string(rateRatio) + "x), exact stacked arrow volleys, two archer sources, projectile immunity, fall damage, melee/explosion cooldowns, four named roles, old billboard/Wolf saves, and Creative immunity passed"
                : "combat regression: " + firstFailure;
    return ok;
}
