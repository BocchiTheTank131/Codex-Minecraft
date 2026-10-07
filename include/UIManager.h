#pragma once
#include "UiLayout.h"
#include "Survival.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

class InputManager;
class Inventory;
class Player;
class SurvivalWorld;
class World;

enum class GameState {
    MainMenu,
    Playing,
    Inventory,
    CraftingTable,
    Furnace,
    Chest,
    PauseMenu,
    Settings,
    AudioSettings,
    Controls,
    ResetWorld,
};

struct RecipeBookView {
    RecipeCategory category = RecipeCategory::All;
    bool craftableOnly = false;
    bool searchFocused = false;
    int page = 0;
    std::string search;
    std::vector<int> entries;
};

class UIManager {
public:
    GameState state() const {
        return state_;
    }
    bool simulationPaused() const;
    bool gameplayInterfaceOpen() const;
    bool craftingTableOpen() const {
        return state_ == GameState::CraftingTable;
    }
    bool furnaceOpen() const { return state_ == GameState::Furnace; }
    bool chestOpen() const { return state_ == GameState::Chest; }
    bool cursorShouldBeCaptured() const {
        return state_ == GameState::Playing;
    }

    void toggleInventory(Inventory& inventory);
    void openInventory();
    void openCraftingTable(const glm::ivec3& position);
    void openFurnace(const glm::ivec3& position);
    void openChest(const glm::ivec3& position);
    void closeGameplayInterface(Inventory& inventory);
    void openPauseMenu();
    void openMainMenu();
    void backFromSettings();
    void openSettings();
    void openAudioSettings();
    void openControls();
    void openResetWorld();
    void resumeGame();
    void handleEscape(Inventory* inventory);
    void handleEscape(Inventory& inventory) { handleEscape(&inventory); }
    static UiRect mainMenuButton(int index, int width, int height);

    bool validateOpenBlock(const World& world, const Player& player, Inventory& inventory);
    void updateInventoryInteraction(const InputManager& input,
                                    int framebufferWidth,
                                    int framebufferHeight,
                                    Inventory& inventory,
                                    SurvivalWorld& survival,
                                    World& world,
                                    const Player& player,
                                    bool creativeMode);

    int hoveredMenuItem(const glm::dvec2& cursor, int width, int height) const;
    const glm::ivec3& openedContainerPosition() const {
        return worldContainerPosition_;
    }
    const RecipeBookView& recipeBook() const { return recipeBook_; }
    static bool runRecipeBookSelfTest(std::string& report);

private:
    GameState state_ = GameState::Playing;
    bool settingsFromMainMenu_ = false;
    glm::ivec3 worldContainerPosition_{0};
    UiHit dragHit_{};
    RecipeBookView recipeBook_{};
    int selectedRecipe_ = -1;
    double lastLeftClickTime_ = -1.0;
    UiHit lastLeftClickHit_{};
    void refreshRecipeBook(const Inventory& inventory, bool tableMode);
    static int menuHit(double mouseX, double mouseY, int width, int height, GameState state);
};
