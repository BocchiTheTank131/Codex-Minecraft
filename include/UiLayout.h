#pragma once

#include <cstddef>
#include <initializer_list>

enum class UiMode { Inventory, CraftingTable, Creative, Furnace, Chest };
enum class UiSlotKind {
    None,
    PlayerInventorySlot,
    CraftingSlot,
    CraftingOutput,
    FurnaceInput,
    FurnaceFuel,
    FurnaceOutput,
    ChestSlot,
    CreativeItem,
    RecipeItem, RecipeCategory, RecipeCraftable, RecipeSearch,
    RecipePrevious, RecipeNext
};

struct UiRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool contains(double mouseX, double mouseY) const {
        return mouseX >= x && mouseX < x + width &&
               mouseY >= y && mouseY < y + height;
    }
};

struct UiHit {
    UiSlotKind kind = UiSlotKind::None;
    int index = -1;
    bool valid() const { return kind != UiSlotKind::None; }
    bool operator==(const UiHit& other) const {
        return kind == other.kind && index == other.index;
    }
    bool operator!=(const UiHit& other) const { return !(*this == other); }
};

namespace UiLayout {
inline constexpr int CreativeColumns = 13;
inline UiRect panel(UiMode mode, int width, int height) {
    const bool creative = mode == UiMode::Creative;
    return {width * .5f - 325.0f, height * .5f - (creative ? 315.0f : 280.0f),
            650.0f, creative ? 630.0f : 560.0f};
}
inline UiRect playerSlot(UiMode mode, int index, int width, int height) {
    const UiRect p = panel(mode, width, height);
    if (index < 0 || index >= 36 || (mode == UiMode::Creative && index >= 9))
        return {};
    if (index < 9)
        return {p.x + 100.0f + index * 48.0f,
                p.y + (mode == UiMode::Creative ? 570.0f : 470.0f), 44.0f, 44.0f};
    const int storage = index - 9;
    return {p.x + 100.0f + (storage % 9) * 48.0f,
            p.y + 300.0f + (storage / 9) * 48.0f, 44.0f, 44.0f};
}
inline UiRect craftingSlot(int index, int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 70.0f + (index % 3) * 48.0f,
            p.y + 80.0f + (index / 3) * 48.0f, 44.0f, 44.0f};
}
inline UiRect craftingOutput(int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 330.0f, p.y + 125.0f, 44.0f, 44.0f};
}
inline UiRect creativeItem(int index, int width, int height) {
    const UiRect p = panel(UiMode::Creative, width, height);
    return {p.x + (p.width - (CreativeColumns * 43.0f - 3.0f)) * .5f +
                (index % CreativeColumns) * 43.0f,
            p.y + 45.0f + (index / CreativeColumns) * 43.0f, 40.0f, 40.0f};
}
inline UiRect furnaceSlot(UiSlotKind kind, int width, int height) {
    const UiRect p = panel(UiMode::Furnace, width, height);
    if (kind == UiSlotKind::FurnaceInput)
        return {p.x + 190.0f, p.y + 90.0f, 44.0f, 44.0f};
    if (kind == UiSlotKind::FurnaceFuel)
        return {p.x + 190.0f, p.y + 190.0f, 44.0f, 44.0f};
    return {p.x + 370.0f, p.y + 140.0f, 44.0f, 44.0f};
}
inline UiRect chestSlot(int index, int width, int height) {
    const UiRect p = panel(UiMode::Chest, width, height);
    return {p.x + 100.0f + (index % 9) * 48.0f,
            p.y + 70.0f + (index / 9) * 48.0f, 44.0f, 44.0f};
}
inline UiRect recipeCategory(int index, int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 390.0f + index * 46.0f, p.y + 42.0f, 44.0f, 26.0f};
}
inline UiRect recipeSearch(int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 390.0f, p.y + 76.0f, 228.0f, 25.0f};
}
inline UiRect recipeCraftable(int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 390.0f, p.y + 107.0f, 228.0f, 25.0f};
}
inline UiRect recipeItem(int index, int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + 394.0f + (index % 4) * 53.0f,
            p.y + 143.0f + (index / 4) * 49.0f, 44.0f, 44.0f};
}
inline UiRect recipePageButton(bool next, int width, int height) {
    const UiRect p = panel(UiMode::Inventory, width, height);
    return {p.x + (next ? 570.0f : 394.0f), p.y + 245.0f, 48.0f, 25.0f};
}
inline UiHit hit(UiMode mode, double mouseX, double mouseY,
                 int width, int height, std::size_t creativeCount = 0,
                 std::size_t recipeCount = 0, int recipePage = 0) {
    if (mode == UiMode::Inventory || mode == UiMode::CraftingTable) {
        for (int index = 0; index < 5; ++index)
            if (recipeCategory(index, width, height).contains(mouseX, mouseY))
                return {UiSlotKind::RecipeCategory, index};
        if (recipeSearch(width, height).contains(mouseX, mouseY))
            return {UiSlotKind::RecipeSearch, 0};
        if (recipeCraftable(width, height).contains(mouseX, mouseY))
            return {UiSlotKind::RecipeCraftable, 0};
        for (int index = 0; index < 8; ++index) {
            if (static_cast<std::size_t>(recipePage * 8 + index) < recipeCount &&
                recipeItem(index, width, height).contains(mouseX, mouseY))
                return {UiSlotKind::RecipeItem, index};
        }
        if (recipePageButton(false, width, height).contains(mouseX, mouseY))
            return {UiSlotKind::RecipePrevious, 0};
        if (recipePageButton(true, width, height).contains(mouseX, mouseY))
            return {UiSlotKind::RecipeNext, 0};
        if (craftingOutput(width, height).contains(mouseX, mouseY))
            return {UiSlotKind::CraftingOutput, 0};
        const int gridSize = mode == UiMode::CraftingTable ? 3 : 2;
        for (int row = 0; row < gridSize; ++row)
            for (int column = 0; column < gridSize; ++column) {
                const int index = row * 3 + column;
                if (craftingSlot(index, width, height).contains(mouseX, mouseY))
                    return {UiSlotKind::CraftingSlot, index};
            }
    }
    if (mode == UiMode::Furnace) {
        for (UiSlotKind kind : {UiSlotKind::FurnaceInput, UiSlotKind::FurnaceFuel,
                                UiSlotKind::FurnaceOutput})
            if (furnaceSlot(kind, width, height).contains(mouseX, mouseY))
                return {kind, 0};
    }
    if (mode == UiMode::Chest) {
        for (int index = 0; index < 27; ++index)
            if (chestSlot(index, width, height).contains(mouseX, mouseY))
                return {UiSlotKind::ChestSlot, index};
    }
    if (mode == UiMode::Creative) {
        for (int index = 0; index < static_cast<int>(creativeCount); ++index)
            if (creativeItem(index, width, height).contains(mouseX, mouseY))
                return {UiSlotKind::CreativeItem, index};
    }
    for (int index = 0; index < 36; ++index)
        if (playerSlot(mode, index, width, height).contains(mouseX, mouseY))
            return {UiSlotKind::PlayerInventorySlot, index};
    return {};
}
} // namespace UiLayout
