#pragma once
#include "Survival.h"
#include <queue>
#include <unordered_map>
class World;
class Weather;

// Plant stages/fill are saved blocks. Unloading pauses session growth timers.
class FarmingSystem {
public:
    bool plant(World&, const glm::ivec3&, Item seed = Item::Seeds);
    void update(float dt, World&, const glm::vec3&, const Weather* weather = nullptr, float daylight = 1);
    bool fertilize(World&, const glm::ivec3&);
    bool interact(World&, Inventory&, SurvivalWorld&, const glm::ivec3& target,
                  const glm::ivec3& adjacent, const glm::ivec3& normal,
                  Item held, bool offhand, bool creative);
    static Block plantedBlock(Item);
    static bool hydrated(const World&, const glm::ivec3& soil);
    static std::vector<ItemStack> harvest(Block, const glm::ivec3&, std::uint32_t seed);
    std::size_t trackedCount() const { return states_.size(); }
    float updateMilliseconds() const { return updateMilliseconds_; }
    const char* feedback() const { return feedback_; }
private:
    struct Hash {std::size_t operator()(const glm::ivec3& p) const {
        auto h=std::hash<int>{}(p.x);h^=std::hash<int>{}(p.y)+0x9e3779b9+(h<<6)+(h>>2);
        return h^(std::hash<int>{}(p.z)+0x9e3779b9+(h<<6)+(h>>2));
    }};
    struct State { double last=0; float growth=0; Block block=Block::Air; std::uint64_t residency=0; };
    struct Due { double time; glm::ivec3 p; bool operator<(const Due& b)const{return time>b.time;} };
    std::unordered_map<glm::ivec3,State,Hash> states_;
    std::priority_queue<Due> due_;
    std::vector<glm::ivec3> discovery_;
    double clock_=0, discoveryTime_=0;
    float updateMilliseconds_=0;
    const char* feedback_=nullptr;
    bool grow(World&, const glm::ivec3&, Block, bool forced);
};

bool consumeFood(Player&, Inventory&, bool offhand, bool creative);
