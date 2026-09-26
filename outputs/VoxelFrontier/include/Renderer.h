#pragma once

#include "Block.h"
#include "Settings.h"
#include "GameMode.h"
#include "Survival.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

class World;
const std::array<std::uint8_t, 7>& uiGlyph(char character);
struct FurnaceData;
struct ChestData;
struct RecipeBookView;
inline constexpr float DayNightCycleSeconds = 420.0f;

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void renderSky(const glm::mat4& view, const glm::mat4& projection, float worldTime) const;
    void renderWorld(const World& world,
                     const glm::mat4& view,
                     const glm::mat4& projection,
                     const glm::vec3& cameraPosition,
                     float worldTime,
                     bool fullbright = false,
                     bool underwater = false,
                     bool spectatorInsideBlock = false) const;
    void renderHud(int width,
                   int height,
                   const Inventory& inventory,
                   float health,
                   float hurtFlash,
                   float breakProgress,
                   const std::string& debugText,
                   const std::string& statusText,
                   bool spectator = false) const;
    void renderSurvivalUi(int width,
                          int height,
                          const Inventory& inventory,
                          float hunger,
                          float xpProgress,
                          int xpLevel,
                          bool fullbright) const;
    void renderInventory(int width,
                         int height,
                         const Inventory& inventory,
                         bool tableMode,
                         const RecipeBookView& recipeBook,
                         double mouseX,
                         double mouseY) const;
    void renderCreativeInventory(int width,
                                 int height,
                                 const Inventory& inventory,
                                 double mouseX,
                                 double mouseY) const;
    void renderFurnace(int width,
                       int height,
                       const Inventory& inventory,
                       const FurnaceData& furnace,
                       double mouseX,
                       double mouseY) const;
    void renderChest(int width,
                     int height,
                     const Inventory& inventory,
                     const ChestData& chest,
                     double mouseX,
                     double mouseY) const;
    void renderMenu(int width,
                    int height,
                    bool settingsPage,
                    int hovered,
                    const GameSettings& settings,
                    GameMode mode) const;
    void renderControlsMenu(int width, int height, int hovered,
                            const GameSettings& settings, int activeBinding) const;
    void renderResetMenu(int width,
                         int height,
                         int hovered,
                         const std::string& seedText,
                         GameMode mode) const;
    void renderHeldItem(int width, int height, const ItemStack& stack, float swing) const;
    void renderUnderwaterOverlay(int width, int height) const;
    void renderEntities(const std::vector<RenderCuboid>& cuboids,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        float worldTime,
                        float maximumDistance) const;
    void renderItemSprites(const std::vector<RenderItemSprite>& sprites,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           float time,
                           float maximumDistance) const;
    void renderSelectionOutline(const glm::ivec3& block, Block type,
                                const glm::mat4& view,
                                const glm::mat4& projection) const;
    void renderMobOutline(const RenderCuboid& bounds,
                          const glm::mat4& view,
                          const glm::mat4& projection) const;
    void spawnBreakParticles(const glm::ivec3& blockPosition, Block block);
    void spawnHitParticles(const glm::vec3& position, bool critical);
    void updateParticles(float deltaTime);
    void renderParticles(const glm::mat4& view,
                         const glm::mat4& projection,
                         float maximumDistance) const;
    void setParticlePercent(int percent) { particlePercent_ = std::clamp(percent, 0, 100); }
    void setEffectQuality(int quality) { effectQuality_ = std::clamp(quality, 0, 2); }
    int visibleEntityCount() const { return visibleEntityCount_; }

private:
    struct Particle {
        glm::vec3 position, velocity, color;
        float life;
    };
    GLuint worldProgram_ = 0, skyProgram_ = 0, uiProgram_ = 0, particleProgram_ = 0,
           entityProgram_ = 0, itemProgram_ = 0;
    GLuint atlasTexture_ = 0, itemTexture_ = 0, skyVao_ = 0, uiVao_ = 0, uiVbo_ = 0,
           particleVao_ = 0, particleVbo_ = 0;
    GLuint entityVao_ = 0, entityVbo_ = 0, itemVao_ = 0, itemVbo_ = 0;
    GLint uiItemAtlasUniform_ = -1;
    GLint itemAtlasUniform_ = -1;
    std::vector<Particle> particles_;
    int particlePercent_ = 100;
    int effectQuality_ = 2;
    mutable int visibleEntityCount_ = 0;
    static GLuint compileShader(GLenum type, const char* source);
    static GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    static GLuint createAtlasTexture();
    static GLuint loadItemTexture(const std::string& path);
};
