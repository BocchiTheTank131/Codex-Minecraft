#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include "Weather.h"
struct GameSettings;
class World;

class WeatherRenderer {
public:
    WeatherRenderer();
    ~WeatherRenderer();
    void render(const Weather& weather, const World& world, const GameSettings& settings,
                const glm::vec3& camera, const glm::mat4& view, const glm::mat4& projection,
                double time, float daylight);
    std::size_t particleCount() const { return particleCount_; }
    void clear();
private:
    struct Vertex { glm::vec3 position; glm::vec4 color; };
    struct Column { int x=0,z=0; bool valid=false; Precipitation kind=Precipitation::None; };
    std::array<Column,4096> columns_{};
    std::vector<Vertex> vertices_;
    GLuint program_=0,vao_=0,vbo_=0;
    GLint vp_=-1;
    std::size_t particleCount_=0;
    void quad(const glm::vec3& center, const glm::vec3& right, const glm::vec3& up, const glm::vec4& color);
};
