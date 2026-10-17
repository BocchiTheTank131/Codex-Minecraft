#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
class World;
class FarmingSystem {
public:
    void plant(World& world, const glm::ivec3& position);
    void update(float deltaTime, World& world, const glm::vec3& playerPosition);

private:
    struct Hash {std::size_t operator()(const glm::ivec3& p)const{
        auto h=std::hash<int>{}(p.x);h^=std::hash<int>{}(p.y)+0x9e3779b9+(h<<6)+(h>>2);
        return h^(std::hash<int>{}(p.z)+0x9e3779b9+(h<<6)+(h>>2));
    }};
    std::unordered_map<glm::ivec3,float,Hash> growth_;
    glm::ivec3 scanOrigin_{0};
    int scanCursor_=0,scanRadius_=0,scanLow_=0,scanHigh_=0;
    float scanTimer_ = 0.f;

};
