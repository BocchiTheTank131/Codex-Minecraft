#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <unordered_map>
class World;
class FarmingSystem {
public:
    void plant(World& world,const glm::ivec3& position);
    void update(float deltaTime,World& world,const glm::vec3& playerPosition);
private:
    std::unordered_map<std::int64_t,float> growth_;
    float scanTimer_=0.f;
    static std::int64_t key(const glm::ivec3& p);
};
