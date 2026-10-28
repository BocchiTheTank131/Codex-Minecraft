#pragma once
#include "Survival.h"
#include <array>
#include <future>
#include <functional>
#include <unordered_set>

class World;
class Player;
struct PlayerInput;
struct Boat {
    std::uint32_t id=0;
    Item item=Item::OakBoat;
    glm::vec3 position{0},velocity{0};
    float heading=0, angularVelocity=0;
};
struct ExplorationMap {
    static constexpr int Size=128;
    std::uint32_t id=0, revision=0;
    int originX=0,originZ=0,scale=0;
    std::array<std::uint32_t,Size*Size> pixels{}; // zero means unexplored
};
struct Waypoint {std::string name; glm::vec3 position{0};};
class Transportation {
    friend class EngineOptimizationTest;
    friend class IconTest;
public:
    explicit Transportation(glm::vec3 spawn):spawn_(spawn){}
    ~Transportation();
    bool placeBoat(World&,Item,const glm::vec3&,float heading);
    bool useBoat(Player&,const World&,const glm::vec3&,const glm::vec3&,float reach);
    bool breakBoat(Player&,Inventory&,SurvivalWorld&,const World&,float reach);
    bool dismount(Player&,const World&);
    void update(float,const PlayerInput&,World&,Player&,const SurvivalWorld&);
    void appendModels(std::vector<RenderCuboid>&,const glm::vec3&,float distance) const;
    bool intersectsBox(glm::vec3 low,glm::vec3 high) const;
    float landingHeight(glm::vec3 low,glm::vec3 high,float previousFeet) const;
    bool mounted() const{return rider_!=0;}
    std::uint32_t createMap(const glm::vec3&,int scale=0);
    std::uint32_t zoomMap(std::uint32_t);
    bool atMapLimit()const{return maps_.size()>=128;}
    std::size_t reclaimMaps(const std::unordered_set<std::uint32_t>& references);
    const ExplorationMap* map(std::uint32_t) const;
    bool refreshMap(std::uint32_t);
    bool refreshing(std::uint32_t id) const{return refreshMap_==id;}
    void reveal(const World&,const Player&,std::uint32_t);
    bool save(const std::string&,std::uint32_t seed) const;
    std::function<bool()> captureSave(const std::string&,std::uint32_t seed) const;
    bool load(const std::string&,std::uint32_t seed);
    const std::vector<Waypoint>& waypoints() const{return waypoints_;}
    int selectedWaypoint=-1;
    bool addWaypoint(std::string,const glm::vec3&);
    bool renameWaypoint(int,std::string);
    void removeWaypoint(int);
    glm::vec3 target() const;
    glm::vec3 originalSpawn() const{return spawn_;}
    float compassAngle=0;
    float mapMilliseconds=0;
    double mapTotalMilliseconds=0,mapWorstMilliseconds=0;
    std::uint64_t mapBatches=0,boatCollisionCandidates=0;
    float boatMilliseconds=0;
    unsigned boatSteps=0;
    const std::vector<Boat>& boats() const{return boats_;}
    static bool selfTest(World&,std::string&);
private:
    glm::vec3 spawn_;
    std::uint32_t nextBoat_=1,nextMap_=1,rider_=0;
    float accumulator_=0,revealTimer_=0;
    bool sneakWasDown_=false;
    std::uint32_t riderRevision_=0;
    std::vector<RenderCuboid> obstacles_;
    std::vector<Boat> boats_;
    struct HullCell {int z,x;std::size_t index;};
    std::vector<HullCell> hullGrid_;
    std::vector<std::vector<std::size_t>> hullCandidates_;
    std::vector<std::size_t> activeBoats_;
    std::uint32_t refreshMap_=0;
    int refreshCursor_=0;
    std::uint32_t surveyMap_=0;
    int surveyX_=0,surveyZ_=0,surveyY_=0,surveyChunks_=0;
    bool surveyComplete_=false;
    std::vector<ExplorationMap> maps_;
    std::vector<Waypoint> waypoints_;
    struct MapResult {std::uint32_t id;std::vector<std::pair<int,std::uint32_t>> pixels;};
    std::future<MapResult> mapJob_;
    void finishMapJob(bool wait);
};
inline bool isBoatItem(Item i){return i>=Item::OakBoat && i<=Item::DarkOakBoat;}
