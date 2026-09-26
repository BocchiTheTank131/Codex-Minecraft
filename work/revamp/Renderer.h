#pragma once

#include "Block.h"

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

    void renderSky(const glm::mat4& view, const glm::mat4& projection, float worldTime) const;
    void renderWorld(const World& world, const glm::mat4& view, const glm::mat4& projection,
                     const glm::vec3& cameraPosition, float worldTime) const;
    void renderHud(int framebufferWidth, int framebufferHeight, int selectedSlot,
                   const std::string& debugText) const;

    void spawnBreakParticles(const glm::ivec3& blockPosition, Block block);
    void updateParticles(float deltaTime);
    void renderParticles(const glm::mat4& view, const glm::mat4& projection) const;

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 color;
        float life;
    };

    GLuint worldProgram_ = 0;
    GLuint skyProgram_ = 0;
    GLuint uiProgram_ = 0;
    GLuint particleProgram_ = 0;
    GLuint atlasTexture_ = 0;
    GLuint skyVao_ = 0;
    GLuint uiVao_ = 0;
    GLuint uiVbo_ = 0;
    GLuint particleVao_ = 0;
    GLuint particleVbo_ = 0;
    std::vector<Particle> particles_;

    static GLuint compileShader(GLenum type, const char* source);
    static GLuint linkProgram(GLuint vertexShader, GLuint fragmentShader);
    static GLuint createAtlasTexture();
};

