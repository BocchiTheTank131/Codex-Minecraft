#pragma once

#include "InputManager.h"
#include "Settings.h"
#include "Survival.h"
#include "GameMode.h"
#include "UIManager.h"
#include "World.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>

struct GLFWwindow;
class FarmingSystem;
class Player;
class Renderer;
class SoundSystem;

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    bool initialize(int argc, char** argv);
    int run();
    void shutdown();

private:
    struct TimingState {
        double previousFrame = 0.0;
        double fpsSampleStart = 0.0;
        double lastSave = 0.0;
        double lastMemorySample = -1.0;
        std::uint64_t workingSetBytes = 0;
        std::uint64_t peakWorkingSetBytes = 0;
        bool memorySampleAvailable = false;
        int framesInSample = 0;
        float fps = 0.0f;
        float worldTime = 35.0f;
        float stepTimer = 0.0f;
        float bobTime = 0.0f;
        float frameMilliseconds = 0.0f;
    };

    struct InteractionState {
        RayHit blockTarget{};
        MobTarget mobTarget{};
        glm::ivec3 miningTarget{0};
        bool hasBlockTarget = false;
        bool hasMiningTarget = false;
        float breakProgress = 0.0f;
        float attackCooldown = 0.0f;
        float heldItemSwing = 0.0f;
        float cameraKickDegrees = 0.0f;
    };

    struct WindowState {
        int windowedX = 100;
        int windowedY = 100;
        int windowedWidth = 1280;
        int windowedHeight = 720;
    };

    struct SmokeTestState {
        bool uiEnabled = false;
        bool survivalEnabled = false;
        bool resetEnabled = false;
        bool worldgenEnabled = false;
        bool spectatorEnabled = false;
        bool billboardPreview = false;
        float previewBrightness = 1.0f;
        bool worldgenScreenshotTaken = false;
        bool checksPassed = true;
        double startTime = 0.0;
        int stage = -1;
        float inventoryStartTime = 0.0f;
        float pauseStartTime = 0.0f;
        float craftingStartTime = 0.0f;
    };

    GLFWwindow* window_ = nullptr;
    GameSettings settings_{};
    InputManager input_{};
    UIManager ui_{};

    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<SoundSystem> sounds_;
    std::string executablePath_;
    std::unique_ptr<World> world_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<Inventory> inventory_;
    std::unique_ptr<SurvivalWorld> survival_;
    std::unique_ptr<FarmingSystem> farming_;

    std::uint32_t seed_ = 20260917;
    TimingState timing_{};
    InteractionState interaction_{};
    WindowState windowState_{};
    SmokeTestState smokeTest_{};

    bool initialized_ = false;
    bool glfwInitialized_ = false;
    bool saveOnExit_ = true;
    bool fullbright_ = false;
    bool showDebug_ = true;
    bool showWorldgenDebug_ = false;
    bool wasInWater_ = false;
    bool screenshotRequested_ = false;
    double saveWarningUntil_ = 0.0;
    bool creativeMode_ = false;
    bool spectatorMode_ = false;
    GameMode resetMode_ = GameMode::Survival;
    float currentFov_ = 74.0f;
    float zoomFov_ = 30.0f;
    int activeSettingsSlider_ = -1;
    int activeControlBinding_ = -1;
    unsigned int multisampleFramebuffer_ = 0;
    unsigned int multisampleColorBuffer_ = 0;
    unsigned int multisampleDepthBuffer_ = 0;
    int multisampleWidth_ = 0;
    int multisampleHeight_ = 0;
    int multisampleSamples_ = 0;
    std::string resetSeedText_;

    void parseArguments(int argc, char** argv);
    bool createWindow();
    void createWorldAndSystems();
    void synchronizeCursorCapture();
    void applyFullscreenSetting();
    void saveAll();
    void saveSettings();
    void saveWorldMetadata() const;
    GameMode gameMode() const;
    void setGameMode(GameMode mode);
    glm::vec3 safeExitFromSpectator() const;
    void resetWorld(std::uint32_t newSeed, GameMode mode);

    float beginFrame();
    void handleGlobalInput();
    void updatePauseInterface();
    void updateSimulation(float deltaTime);
    void updateTargets();
    void updatePlayingInteraction(float deltaTime);
    void updateMobAttack();
    void updateMining(float deltaTime);
    void handleUseAction();
    void handleDropAction();
    void damageSelectedToolWithSound();
    void finishSimulationFrame(float deltaTime, float oldHealth);

    void renderFrame(float deltaTime);
    bool zoomActive() const;
    void prepareRenderTarget(int width, int height);
    void resolveRenderTarget(int width, int height);
    void destroyRenderTarget();
    std::string buildDebugText() const;
    bool playerIsWalking() const;

    void runSurvivalSmokeTest();
    void runSpectatorSmokeTest();
    void runResetSmokeTest();
    void updateUiSmokeTest(double now);
};
