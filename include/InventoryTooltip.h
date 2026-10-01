#pragma once

#include "Definitions.h"
#include "UIManager.h"
#include "UiLayout.h"
#include "World.h"
#include <algorithm>

namespace InventoryTooltip {
// Share the clickable slot layout and authoritative item display names.
inline Item hoveredItem(UiMode mode, int width, int height, double x, double y,
                        const Inventory& inventory, const RecipeBookView* book = nullptr,
                        const FurnaceData* furnace = nullptr, const ChestData* chest = nullptr) {
    const auto& catalog = creativeCatalog();
    const UiHit hit = UiLayout::hit(mode, x, y, width, height, catalog.size(),
                                   book ? book->entries.size() : 0, book ? book->page : 0);
    const auto itemOf=[](const ItemStack& stack) { return stack.empty()?Item::None:stack.item; };
    switch (hit.kind) {
    case UiSlotKind::PlayerInventorySlot: return itemOf(inventory.slot(hit.index));
    case UiSlotKind::CreativeItem: return catalog[hit.index];
    case UiSlotKind::CraftingSlot: return itemOf(inventory.craftSlot(hit.index));
    case UiSlotKind::CraftingOutput:
        return itemOf(inventory.craftingOutput(mode == UiMode::CraftingTable));
    case UiSlotKind::RecipeItem:
        if (book && hit.index >= 0 && book->page*8+hit.index < static_cast<int>(book->entries.size()))
            return craftingRecipes()[book->entries[book->page*8+hit.index]].output;
        return Item::None;
    case UiSlotKind::FurnaceInput: return furnace ? itemOf(furnace->input) : Item::None;
    case UiSlotKind::FurnaceFuel: return furnace ? itemOf(furnace->fuel) : Item::None;
    case UiSlotKind::FurnaceOutput: return furnace ? itemOf(furnace->output) : Item::None;
    case UiSlotKind::ChestSlot: return chest ? itemOf(chest->slots[hit.index]) : Item::None;
    default: return Item::None;
    }
}
inline UiRect bounds(int width, int height, double mouseX, double mouseY, float textWidth) {
    const float w = std::min(textWidth + 16.0f, static_cast<float>(width)-8.0f);
    constexpr float h = 26.0f;
    float x = static_cast<float>(mouseX)+16.0f, y = static_cast<float>(mouseY)+18.0f;
    if (x+w>width-4) x=static_cast<float>(mouseX)-w-12.0f;
    if (y+h>height-4) y=static_cast<float>(mouseY)-h-12.0f;
    return {std::clamp(x,4.0f,std::max(4.0f,width-w-4.0f)),
            std::clamp(y,4.0f,std::max(4.0f,height-h-4.0f)),w,h};
}
}
