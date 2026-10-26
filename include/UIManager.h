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
struct GameSettings;

enum class GameState {
    MainMenu,
    WorldSelection,
    CreateWorld,
    Playing,
    Inventory,
    CraftingTable,
    Furnace,
    Chest,
    PauseMenu,
    Settings,
    VideoSettings,
    AudioSettings,
    Controls,
    ResetWorld,
    WeatherSettings,
};

struct RecipeBookView {
    std::string usesKey="U";
    Item ingredientItem=Item::None;
    RecipeCategory category = RecipeCategory::All;
    bool craftableOnly = false;
    bool searchFocused = false;
    int page = 0;
    std::string search;
    std::vector<int> entries;
};

class UIManager {
    friend class IconTest;
    friend class QolTest;
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
    void openWorldSelection() { state_ = GameState::WorldSelection; }
    void openCreateWorld() { state_ = GameState::CreateWorld; }
    void openWeatherSettings() { state_=GameState::WeatherSettings; }
    void openVideoSettings() { state_ = GameState::VideoSettings; }
    void backFromSettings();
    void openSettings();
    void openAudioSettings();
    void openControls();
    void openResetWorld();
    void resumeGame();
    void handleEscape(Inventory* inventory);
    void handleEscape(Inventory& inventory) { handleEscape(&inventory); }
    static UiRect mainMenuButton(int index, int width, int height);
    static float menuScale(int width, int height);
    static inline int scalePreference=0;
    void setPreferences(GameSettings* settings){preferences_=settings;}
    const GameSettings* preferences()const{return preferences_;}
    int creativeCategory()const{return creativeCategory_;}
    void rememberItem(Item);

    bool validateOpenBlock(const World& world, const Player& player, Inventory& inventory);
    void updateInventoryInteraction(InputManager& input,
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
    int creativePage() const {return creativePage_;}
    const std::vector<Item>& creativeItems() const {return creativeSearch_.empty()&&creativeCategory_==0?creativeCatalog():creativeFiltered_;}
    const std::string& creativeSearch() const {return creativeSearch_;}
    bool creativeSearchFocused() const {return creativeSearchFocused_;}
    const RecipeBookView& recipeBook() const { return recipeBook_; }
    static bool runRecipeBookSelfTest(std::string& report);

private:
    GameState state_ = GameState::Playing;
    bool settingsFromMainMenu_ = false;
    glm::ivec3 worldContainerPosition_{0};
    UiHit dragHit_{};
    bool inventoryLeftWasDown_ = false;
    UiHit inventoryLeftDragStart_{};
    RecipeBookView recipeBook_{};
    int selectedRecipe_ = -1;
    int creativePage_=0;
    int creativeCategory_=0;
    GameSettings* preferences_=nullptr;
    std::string creativeSearch_;
    std::vector<Item> creativeFiltered_;
    bool creativeSearchFocused_=false;
    void refreshCreativeSearch();
    double lastLeftClickTime_ = -1.0;
    UiHit lastLeftClickHit_{};
    void refreshRecipeBook(const Inventory& inventory, bool tableMode);
    static int menuHit(double mouseX, double mouseY, int width, int height, GameState state);
};
