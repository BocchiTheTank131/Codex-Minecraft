#pragma once

#include "Block.h"
#include "Settings.h"
#include "Survival.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class World;

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void renderSky(const glm::mat4& view,const glm::mat4& projection,float worldTime) const;
    void renderWorld(const World& world,const glm::mat4& view,const glm::mat4& projection,
                     const glm::vec3& cameraPosition,float worldTime,bool fullbright=false,bool underwater=false) const;
    void renderHud(int width,int height,const Inventory& inventory,float health,float hurtFlash,
                   float breakProgress,const std::string& debugText,const std::string& statusText) const;
    void renderSurvivalUi(int width,int height,const Inventory& inventory,float hunger,float xpProgress,int xpLevel,bool fullbright) const;
    void renderInventory(int width,int height,const Inventory& inventory,bool tableMode,double mouseX,double mouseY) const;
    void renderMenu(int width,int height,bool settingsPage,int hovered,const GameSettings& settings) const;
    void renderHeldItem(int width,int height,const ItemStack& stack,float swing) const;
    void renderUnderwaterOverlay(int width,int height) const;
    void renderEntities(const std::vector<RenderCuboid>& cuboids,const glm::mat4& view,const glm::mat4& projection,float worldTime) const;
    void renderItemSprites(const std::vector<RenderItemSprite>& sprites,const glm::mat4& view,const glm::mat4& projection,float time) const;
    void renderSelectionOutline(const glm::ivec3& block,const glm::mat4& view,const glm::mat4& projection) const;
    void spawnBreakParticles(const glm::ivec3& blockPosition,Block block);
    void updateParticles(float deltaTime);
    void renderParticles(const glm::mat4& view,const glm::mat4& projection) const;

private:
    struct Particle { glm::vec3 position,velocity,color; float life; };
    GLuint worldProgram_=0,skyProgram_=0,uiProgram_=0,particleProgram_=0,entityProgram_=0,itemProgram_=0;
    GLuint atlasTexture_=0,itemTexture_=0,skyVao_=0,uiVao_=0,uiVbo_=0,particleVao_=0,particleVbo_=0;
    GLuint entityVao_=0,entityVbo_=0,itemVao_=0,itemVbo_=0;
    std::vector<Particle> particles_;
    static GLuint compileShader(GLenum type,const char* source);
    static GLuint linkProgram(GLuint vertexShader,GLuint fragmentShader);
    static GLuint createAtlasTexture();
    static GLuint loadItemTexture(const std::string& path);
};
