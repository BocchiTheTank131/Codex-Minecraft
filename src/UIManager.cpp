#include "UIManager.h"

#include "InputManager.h"
#include "Player.h"
#include "Survival.h"
#include "World.h"
#include "Definitions.h"
#include "MenuLayout.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>

bool UIManager::simulationPaused() const {
    return state_ == GameState::MainMenu || state_ == GameState::WorldSelection ||
           state_ == GameState::CreateWorld || state_ == GameState::VideoSettings ||
           state_ == GameState::PauseMenu || state_ == GameState::Settings ||
           state_ == GameState::AudioSettings ||
           state_ == GameState::Controls || state_ == GameState::ResetWorld;
}

bool UIManager::gameplayInterfaceOpen() const {
    return state_ == GameState::Inventory || state_ == GameState::CraftingTable ||
           state_ == GameState::Furnace || state_ == GameState::Chest;
}

void UIManager::toggleInventory(Inventory& inventory) {
    if (gameplayInterfaceOpen()) {
        closeGameplayInterface(inventory);
    } else if (state_ == GameState::Playing) {
        openInventory();
    }
}

void UIManager::openInventory() {
    state_ = GameState::Inventory;
    dragHit_ = {};
    selectedRecipe_ = -1;
    recipeBook_.searchFocused = false;
}

void UIManager::openCraftingTable(const glm::ivec3& position) {
    state_ = GameState::CraftingTable;
    worldContainerPosition_ = position;
    dragHit_ = {};
    selectedRecipe_ = -1;
    recipeBook_.searchFocused = false;
}

void UIManager::openFurnace(const glm::ivec3& position) {
    state_ = GameState::Furnace;
    worldContainerPosition_ = position;
    dragHit_ = {};
}

void UIManager::openChest(const glm::ivec3& position) {
    state_ = GameState::Chest;
    worldContainerPosition_ = position;
    dragHit_ = {};
}

void UIManager::closeGameplayInterface(Inventory& inventory) {
    if (!gameplayInterfaceOpen()) {
        return;
    }
    if (state_ == GameState::Inventory || state_ == GameState::CraftingTable)
        inventory.returnCraftingItems();
    state_ = GameState::Playing;
    dragHit_ = {};
    recipeBook_.searchFocused = false;
}

void UIManager::refreshRecipeBook(const Inventory& inventory, bool tableMode) {
    recipeBook_.entries.clear();
    const auto& all = craftingRecipes();
    std::string search = recipeBook_.search;
    std::transform(search.begin(), search.end(), search.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (int index = 0; index < static_cast<int>(all.size()); ++index) {
        const RecipeInfo& recipe = all[static_cast<std::size_t>(index)];
        if ((recipe.table && !tableMode) || recipe.w > (tableMode ? 3 : 2) ||
            recipe.h > (tableMode ? 3 : 2))
            continue;
        if (recipeBook_.category != RecipeCategory::All &&
            recipe.category != recipeBook_.category)
            continue;
        if (recipeBook_.craftableOnly && !inventory.recipeCraftable(index, tableMode))
            continue;
        std::string name = itemDefinition(recipe.output).displayName;
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!search.empty() && name.find(search) == std::string::npos)
            continue;
        recipeBook_.entries.push_back(index);
    }
    recipeBook_.page = std::clamp(recipeBook_.page, 0,
        std::max(0, (static_cast<int>(recipeBook_.entries.size()) - 1) / 8));
}

bool UIManager::runRecipeBookSelfTest(std::string& report) {
    Inventory inventory;
    inventory.clear();
    inventory.add(Item::Cobblestone, 8);
    UIManager ui;
    ui.openInventory();
    ui.recipeBook_.search = "furnace";
    ui.refreshRecipeBook(inventory, false);
    bool passed = ui.recipeBook_.entries.empty();
    ui.openCraftingTable({0, 0, 0});
    ui.recipeBook_.craftableOnly = true;
    ui.refreshRecipeBook(inventory, true);
    passed &= ui.recipeBook_.entries.size() == 1;
    if (!ui.recipeBook_.entries.empty()) {
        const int index = ui.recipeBook_.entries[0];
        passed &= craftingRecipes()[static_cast<std::size_t>(index)].output == Item::Furnace &&
                  inventory.fillRecipe(index, true, false) &&
                  inventory.craftingOutput(true).item == Item::Furnace;
    }
    ui.recipeBook_.search = "missing recipe";
    ui.refreshRecipeBook(inventory, true);
    passed &= ui.recipeBook_.entries.empty();
    ui.recipeBook_.search.clear();
    ui.recipeBook_.craftableOnly = false;
    ui.recipeBook_.page = 999;
    ui.refreshRecipeBook(inventory, true);
    passed &= ui.recipeBook_.entries.size() > 8 &&
              ui.recipeBook_.page == (static_cast<int>(ui.recipeBook_.entries.size()) - 1) / 8;
    ui.recipeBook_.category = RecipeCategory::Tools;
    ui.refreshRecipeBook(inventory, true);
    passed &= !ui.recipeBook_.entries.empty();
    for (int index : ui.recipeBook_.entries)
        passed &= craftingRecipes()[static_cast<std::size_t>(index)].category == RecipeCategory::Tools;
    ui.recipeBook_.category = RecipeCategory::All;
    for (Item item : {Item::Paper, Item::Book, Item::Bookshelf, Item::Clay, Item::Bricks,
                     Item::SnowBlock, Item::CoalBlock, Item::HayBale, Item::Sandstone,
                     Item::PolishedGranite, Item::MossyCobblestone}) {
        ui.recipeBook_.search = itemDefinition(item).displayName;
        std::transform(ui.recipeBook_.search.begin(), ui.recipeBook_.search.end(),
                       ui.recipeBook_.search.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        ui.recipeBook_.craftableOnly = false;
        ui.refreshRecipeBook(inventory, true);
        bool found = false;
        for (int index : ui.recipeBook_.entries)
            found |= craftingRecipes()[static_cast<std::size_t>(index)].output == item;
        passed &= found;
    }
    report = passed ? "grid filtering, craftable toggle, search, pagination, and autofill passed"
                    : "recipe book regression";
    return passed;
}

void UIManager::openPauseMenu() {
    settingsFromMainMenu_ = false;
    state_ = GameState::PauseMenu;
}

void UIManager::openMainMenu() {
    state_ = GameState::MainMenu;
    settingsFromMainMenu_ = true;
}

void UIManager::backFromSettings() {
    state_ = settingsFromMainMenu_ ? GameState::MainMenu : GameState::PauseMenu;
}

void UIManager::openSettings() {
    state_ = GameState::Settings;
}

void UIManager::openAudioSettings() {
    state_ = GameState::AudioSettings;
}

void UIManager::openControls() {
    state_ = GameState::Controls;
}

void UIManager::openResetWorld() {
    state_ = GameState::ResetWorld;
}

void UIManager::resumeGame() {
    state_ = GameState::Playing;
}

void UIManager::handleEscape(Inventory* inventory) {
    switch (state_) {
    case GameState::Inventory:
    case GameState::CraftingTable:
    case GameState::Furnace:
    case GameState::Chest:
        if (inventory) closeGameplayInterface(*inventory);
        break;
    case GameState::Settings:
    case GameState::ResetWorld:
        backFromSettings();
        break;
    case GameState::Controls:
    case GameState::AudioSettings:
    case GameState::VideoSettings:
        state_ = GameState::Settings;
        break;
    case GameState::WorldSelection:
        openMainMenu();
        break;
    case GameState::CreateWorld:
        openWorldSelection();
        break;
    case GameState::PauseMenu:
        state_ = GameState::Playing;
        break;
    case GameState::Playing:
        openPauseMenu();
        break;
    case GameState::MainMenu:
        break;
    }
}

bool UIManager::validateOpenBlock(const World& world,
                                  const Player& player,
                                  Inventory& inventory) {
    if (!craftingTableOpen() && !furnaceOpen() && !chestOpen()) {
        return true;
    }

    const glm::ivec3 position = worldContainerPosition_;
    const Block expected = craftingTableOpen()
                               ? Block::CraftingTable
                               : (furnaceOpen() ? Block::Furnace : Block::Chest);
    const Block block = world.getBlock(position.x, position.y, position.z);
    const glm::vec3 center = glm::vec3(position) + glm::vec3(0.5f);
    if (block == expected && glm::distance(player.position(), center) <= 6.0f) {
        return true;
    }

    closeGameplayInterface(inventory);
    return false;
}

void UIManager::updateInventoryInteraction(const InputManager& input,
                                           int framebufferWidth,
                                           int framebufferHeight,
                                           Inventory& inventory,
                                           SurvivalWorld& survival,
                                           World& world,
                                           const Player& player,
                                           bool creativeMode) {
    const glm::dvec2 cursor = input.framebufferCursorPosition();
    const bool tableMode = craftingTableOpen();
    const bool creativeInventory = creativeMode && state_ == GameState::Inventory;
    if (!creativeInventory && (state_ == GameState::Inventory || tableMode)) {
        if (recipeBook_.searchFocused) {
            const int key = input.firstPressedKey();
            if (key == GLFW_KEY_BACKSPACE && !recipeBook_.search.empty())
                recipeBook_.search.pop_back();
            else if (key == GLFW_KEY_ENTER)
                recipeBook_.searchFocused = false;
            else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z &&
                     recipeBook_.search.size() < 18)
                recipeBook_.search.push_back(static_cast<char>('a' + key - GLFW_KEY_A));
            else if (key == GLFW_KEY_SPACE && recipeBook_.search.size() < 18)
                recipeBook_.search.push_back(' ');
        }
        refreshRecipeBook(inventory, tableMode);
    }
    const UiMode mode = furnaceOpen() ? UiMode::Furnace
                      : chestOpen() ? UiMode::Chest
                      : creativeInventory ? UiMode::Creative
                      : tableMode ? UiMode::CraftingTable : UiMode::Inventory;
    const UiHit hit = UiLayout::hit(mode, cursor.x, cursor.y, framebufferWidth,
                                   framebufferHeight, creativeCatalog().size(),
                                   recipeBook_.entries.size(), recipeBook_.page);
    const bool leftPressed = input.mousePressed(GLFW_MOUSE_BUTTON_LEFT);
    const bool rightPressed = input.mousePressed(GLFW_MOUSE_BUTTON_RIGHT);
    const bool rightDown = input.mouseDown(GLFW_MOUSE_BUTTON_RIGHT);
    const bool shiftDown = input.actionDown(ControlAction::Sneak);

    if ((leftPressed || rightPressed) &&
        (state_ == GameState::Inventory || tableMode) && !creativeInventory) {
        bool bookClick = true;
        switch (hit.kind) {
        case UiSlotKind::RecipeCategory:
            recipeBook_.category = static_cast<RecipeCategory>(hit.index);
            recipeBook_.page = 0;
            recipeBook_.searchFocused = false;
            break;
        case UiSlotKind::RecipeCraftable:
            recipeBook_.craftableOnly = !recipeBook_.craftableOnly;
            recipeBook_.page = 0;
            recipeBook_.searchFocused = false;
            break;
        case UiSlotKind::RecipeSearch:
            recipeBook_.searchFocused = true;
            break;
        case UiSlotKind::RecipePrevious:
            recipeBook_.page = std::max(0, recipeBook_.page - 1);
            recipeBook_.searchFocused = false;
            break;
        case UiSlotKind::RecipeNext:
            recipeBook_.page = std::min(
                std::max(0, (static_cast<int>(recipeBook_.entries.size()) - 1) / 8),
                recipeBook_.page + 1);
            recipeBook_.searchFocused = false;
            break;
        case UiSlotKind::RecipeItem: {
            const int visibleIndex = recipeBook_.page * 8 + hit.index;
            if (visibleIndex < static_cast<int>(recipeBook_.entries.size())) {
                selectedRecipe_ = recipeBook_.entries[static_cast<std::size_t>(visibleIndex)];
                inventory.fillRecipe(selectedRecipe_, tableMode, shiftDown);
            }
            recipeBook_.searchFocused = false;
            break;
        }
        default:
            bookClick = false;
            if (leftPressed || rightPressed)
                recipeBook_.searchFocused = false;
            break;
        }
        if (bookClick) {
            refreshRecipeBook(inventory, tableMode);
            dragHit_ = hit;
            return;
        }
    }

    auto handleContainerClick = [&](bool rightClick) {
        if (furnaceOpen()) {
            FurnaceData* furnace = world.furnaceAt(worldContainerPosition_, true);
            if (!furnace)
                return false;
            if (hit.kind == UiSlotKind::FurnaceInput) {
                if (shiftDown)
                    return inventory.moveExternalToInventory(furnace->input);
                if (!inventory.cursorStack().empty() &&
                    smeltingResult(inventory.cursorStack().item) == Item::None)
                    return true;
                return inventory.clickExternalSlot(furnace->input, rightClick);
            }
            if (hit.kind == UiSlotKind::FurnaceFuel) {
                if (shiftDown)
                    return inventory.moveExternalToInventory(furnace->fuel);
                if (!inventory.cursorStack().empty() && furnaceFuelSeconds(inventory.cursorStack().item) <= 0.0f)
                    return true;
                return inventory.clickExternalSlot(furnace->fuel, rightClick);
            }
            if (hit.kind == UiSlotKind::FurnaceOutput)
                return shiftDown ? inventory.moveExternalToInventory(furnace->output)
                                 : inventory.clickExternalSlot(furnace->output, rightClick, true);
        }
        if (chestOpen() && hit.kind == UiSlotKind::ChestSlot) {
            ChestData* chest = world.chestAt(worldContainerPosition_, true);
            if (!chest)
                return false;
            ItemStack& stack = chest->slots[static_cast<std::size_t>(hit.index)];
            return shiftDown ? inventory.moveExternalToInventory(stack)
                             : inventory.clickExternalSlot(stack, rightClick);
        }
        if (hit.kind != UiSlotKind::PlayerInventorySlot)
            return false;
        const int playerSlot = hit.index;
        if (shiftDown && chestOpen()) {
            ChestData* chest = world.chestAt(worldContainerPosition_, true);
            return chest && inventory.movePlayerToExternal(
                                playerSlot, chest->slots.data(), static_cast<int>(chest->slots.size()));
        }
        if (shiftDown && furnaceOpen()) {
            FurnaceData* furnace = world.furnaceAt(worldContainerPosition_, true);
            if (!furnace)
                return false;
            const Item item = inventory.slot(playerSlot).item;
            if (furnaceFuelSeconds(item) > 0.0f)
                return inventory.movePlayerToExternal(playerSlot, &furnace->fuel, 1);
            if (smeltingResult(item) != Item::None)
                return inventory.movePlayerToExternal(playerSlot, &furnace->input, 1);
            return true;
        }
        return inventory.clickPlayerSlot(playerSlot, rightClick);
    };

    if (leftPressed || rightPressed) {
        const bool doubleClick = leftPressed && !shiftDown &&
            (hit.kind == UiSlotKind::PlayerInventorySlot ||
             hit.kind == UiSlotKind::CraftingSlot) &&
            hit == lastLeftClickHit_ && glfwGetTime() - lastLeftClickTime_ < 0.28;
        if (leftPressed) {
            lastLeftClickHit_ = hit;
            lastLeftClickTime_ = glfwGetTime();
        }
        if (doubleClick && inventory.gatherMatchingToCursor()) {
            dragHit_ = hit;
            return;
        }
        if (shiftDown && !creativeInventory && selectedRecipe_ >= 0 &&
            hit.kind == UiSlotKind::PlayerInventorySlot &&
            inventory.shiftIngredientToCrafting(hit.index, selectedRecipe_, tableMode)) {
            dragHit_ = hit;
            return;
        }
        const bool handled =
            furnaceOpen() || chestOpen()
                ? handleContainerClick(rightPressed)
                : creativeInventory
                ? inventory.handleCreativeClick(cursor.x, cursor.y, framebufferWidth,
                                                framebufferHeight, rightPressed)
                : inventory.handleInventoryClick(cursor.x, cursor.y, framebufferWidth,
                                                 framebufferHeight, rightPressed, tableMode,
                                                 shiftDown);
        dragHit_ = hit;
        if (!handled && !inventory.cursorStack().empty() &&
            !UiLayout::panel(mode, framebufferWidth, framebufferHeight)
                 .contains(cursor.x, cursor.y)) {
            const ItemStack dropped = inventory.takeCursor(rightPressed);
            survival.spawnDrop(player.cameraPosition() + player.lookDirection() * 1.1f,
                               dropped.item, dropped.count, dropped.durability);
        }
    } else if (rightDown && hit.valid() && hit != dragHit_) {
        if (furnaceOpen() || chestOpen()) {
            handleContainerClick(true);
        } else if (creativeInventory) {
            inventory.handleCreativeClick(
                cursor.x, cursor.y, framebufferWidth, framebufferHeight, true);
        } else {
            inventory.handleInventoryClick(
                cursor.x, cursor.y, framebufferWidth, framebufferHeight, true, tableMode, false);
        }
        dragHit_ = hit;
    }

    if (!rightDown)
        dragHit_ = {};
}

int UIManager::hoveredMenuItem(const glm::dvec2& cursor, int width, int height) const {
    const float scale = state_ == GameState::MainMenu ? 1.0f : menuScale(width, height);
    return menuHit(cursor.x / scale, cursor.y / scale,
                   static_cast<int>(width / scale), static_cast<int>(height / scale), state_);
}



int UIManager::menuHit(
    double mouseX, double mouseY, int width, int height, GameState state) {
    if (state == GameState::MainMenu) {
        for (int index = 0; index < 3; ++index)
            if (mainMenuButton(index, width, height).contains(mouseX, mouseY)) return index;
        return -1;
    }
    if (state == GameState::WorldSelection) {
        for (int i=0; i<6; ++i) if (MenuLayout::worldRow(i,width,height).contains(mouseX,mouseY)) return i;
        for (int i=0; i<3; ++i) if (MenuLayout::worldAction(i,width,height).contains(mouseX,mouseY)) return 10+i;
        for (int i=0; i<2; ++i) if (MenuLayout::pageButton(i,width,height).contains(mouseX,mouseY)) return 13+i;
        return -1;
    }
    if (state == GameState::CreateWorld) {
        for (int i=0; i<3; ++i) if (MenuLayout::creationField(i,width,height).contains(mouseX,mouseY)) return i;
        for (int i=0; i<2; ++i) if (MenuLayout::creationButton(i,width,height).contains(mouseX,mouseY)) return 3+i;
        return -1;
    }
    if (state == GameState::Settings) {
        for (int i=0; i<4; ++i) if (MenuLayout::hubRow(i,width,height).contains(mouseX,mouseY)) return 30+i;
        return -1;
    }
    if (state == GameState::VideoSettings) {
        for (int i=0; i<3; ++i) if (MenuLayout::tab(i,width,height).contains(mouseX,mouseY)) return 30+i;
        for (int i=0; i<12; ++i) if (MenuLayout::videoRow(i,width,height).contains(mouseX,mouseY)) return MenuLayout::VideoActions[i];
        return MenuLayout::back(width,height).contains(mouseX,mouseY) ? 15 : -1;
    }
    const float panelX = width * 0.5f - 220.0f;
    const float panelY = height * 0.5f - 300.0f;
    if (state == GameState::PauseMenu) {
        for (int index = 0; index < 7; ++index) {
            const float y = panelY + 82.0f + index * 61.0f;
            if (mouseX >= panelX + 45.0f && mouseX < panelX + 395.0f && mouseY >= y &&
                mouseY < y + 44.0f)
                return index;
        }
        return -1;
    }
    if (state == GameState::ResetWorld) {
        const float modeY = panelY + 230.0f;
        const float confirmY = panelY + 330.0f;
        const float cancelY = panelY + 400.0f;
        if (mouseX >= panelX + 45.0f && mouseX < panelX + 395.0f) {
            if (mouseY >= modeY && mouseY < modeY + 48.0f)
                return 0;
            if (mouseY >= confirmY && mouseY < confirmY + 48.0f)
                return 1;
            if (mouseY >= cancelY && mouseY < cancelY + 48.0f)
                return 2;
        }
        return -1;
    }
    if (state == GameState::Controls) {
        if (MenuLayout::sensitivity(width,height).contains(mouseX,mouseY)) return ControlActionCount+2;
        const float controlsY = height * 0.5f - 350.0f;
        for (int index = 0; index < ControlActionCount; ++index) {
            const float y = controlsY + 75.0f + index * 32.0f;
            if (mouseX >= panelX + 35.0f && mouseX < panelX + 405.0f &&
                mouseY >= y && mouseY < y + 29.0f)
                return index;
        }
        if (mouseX >= panelX + 45.0f && mouseX < panelX + 395.0f) {
            if (mouseY >= controlsY + 590.0f && mouseY < controlsY + 630.0f)
                return ControlActionCount;
            if (mouseY >= controlsY + 644.0f && mouseY < controlsY + 684.0f)
                return ControlActionCount + 1;
        }
        return -1;
    }
    if (state == GameState::AudioSettings) {
        const float audioY = height * 0.5f - 220.0f;
        for (int index = 0; index < 5; ++index) {
            const float y = audioY + 70.0f + index * 58.0f;
            if (mouseX >= panelX + 24.0f && mouseX < panelX + 416.0f &&
                mouseY >= y && mouseY < y + 46.0f)
                return index;
        }
        if (mouseX >= panelX + 95.0f && mouseX < panelX + 345.0f &&
            mouseY >= audioY + 372.0f && mouseY < audioY + 416.0f)
            return 5;
        return -1;
    }
    return -1;
}

UiRect UIManager::mainMenuButton(int index, int width, int height) {
    const float scale = std::min({1.0f, width / 520.0f, height / 440.0f});
    return {width * .5f - 195.0f * scale,
            height * .5f + (-8.0f + index * 74.0f) * scale,
            390.0f * scale, 54.0f * scale};
}

float UIManager::menuScale(int width, int height) {
    return std::max(.1f, std::min({1.0f, width / 760.0f, height / 760.0f}));
}
