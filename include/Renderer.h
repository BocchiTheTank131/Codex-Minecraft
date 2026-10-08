#pragma once

#include "Block.h"
#include "Settings.h"
#include "GameMode.h"
#include "Survival.h"
#include "ChatUI.h"
#include "WorldLibrary.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <memory>
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
                     float brightness = 0.5f,
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
    void renderChat(int width, int height, const ChatUI& chat, double now) const;
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
                    GameMode mode,
                    bool standaloneDemo,
                    bool showSaveWarning,
                    bool audioPage) const;
    void renderMainMenu(int width, int height, int hovered, bool pressed,
                        bool buttonsVisible, bool standaloneDemo, double now) const;
    void renderSettingsCategories(int width, int height, int hovered,
                                  const GameSettings& settings, bool video) const;
    void renderWorldMenu(int width, int height, int hovered, bool creating,
                         const std::vector<SavedWorld>& worlds, int page, int selected,
                         const std::string& name, const std::string& seed, GameMode mode,
                         int field, const std::string& message, double now) const;
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
    void renderBillboards(const std::vector<RenderBillboard>& billboards,
                          const World& world, const glm::mat4& view,
                          const glm::mat4& projection, float worldTime,
                          float brightness, bool fullbright,
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
    void spawnExplosionParticles(const glm::vec3& position);
    void updateParticles(float deltaTime);
    void clearParticles() { particles_.clear(); }
    void renderParticles(const glm::mat4& view,
                         const glm::mat4& projection,
                         float maximumDistance) const;
    void setParticlePercent(int percent) { particlePercent_ = std::clamp(percent, 0, 100); }
    void setEffectQuality(int quality) { effectQuality_ = std::clamp(quality, 0, 2); }
    int visibleEntityCount() const { return visibleEntityCount_; }

private:
    struct TextGeometryCache;
    std::unique_ptr<TextGeometryCache> textGeometry_;
    struct ProgramUniforms {
        GLint atlas = -1;
        GLint atlasTiles = -1;
        GLint brightness = -1;
        GLint cameraPosition = -1;
        GLint color = -1;
        GLint daylight = -1;
        GLint effectQuality = -1;
        GLint fullbright = -1;
        GLint hurt = -1;
        GLint inverseViewProjection = -1;
        GLint itemAtlas = -1;
        GLint light = -1;
        GLint model = -1;
        GLint opacity = -1;
        GLint projection = -1;
        GLint skyColor = -1;
        GLint spectatorInsideBlock = -1;
        GLint sprite = -1;
        GLint sunDirection = -1;
        GLint time = -1;
        GLint underwater = -1;
        GLint view = -1;
        GLint waterPass = -1;
    };
    static ProgramUniforms cacheUniforms(GLuint program);
    ProgramUniforms billboardUniforms_;
    ProgramUniforms entityUniforms_;
    ProgramUniforms itemUniforms_;
    ProgramUniforms panoramaUniforms_;
    ProgramUniforms particleUniforms_;
    ProgramUniforms skyUniforms_;
    ProgramUniforms uiUniforms_;
    ProgramUniforms worldUniforms_;

    struct Particle {
        glm::vec3 position, velocity, color;
        float life;
    };
    GLuint worldProgram_ = 0, skyProgram_ = 0, uiProgram_ = 0, particleProgram_ = 0,
           entityProgram_ = 0, itemProgram_ = 0, billboardProgram_ = 0;
    GLuint atlasTexture_ = 0, itemTexture_ = 0, skyVao_ = 0, uiVao_ = 0, uiVbo_ = 0,
           particleVao_ = 0, particleVbo_ = 0;
    GLuint entityVao_ = 0, entityVbo_ = 0, itemVao_ = 0, itemVbo_ = 0;
    GLuint hudVao_ = 0, hudVbo_ = 0;
    std::array<GLuint, 4> hostileTextures_{};
    std::array<float, 4> hostileAspectRatios_{};
    GLuint panoramaTexture_ = 0;
    float panoramaAspectRatio_ = 1.0f;
    GLuint panoramaProgram_ = 0;
    mutable std::array<float, 3> menuHover_{};
    mutable double lastMenuDraw_ = 0.0;
    void drawMenuVertices(const void* data, std::size_t count) const;
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
