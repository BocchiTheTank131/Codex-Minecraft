#pragma once
#include "Block.h"
#include <glm/glm.hpp>
#include <map>
#include <set>
#include <vector>
#include <iosfwd>
#include <functional>
#include <array>
class World;
class SurvivalWorld;
class Player;
class SoundSystem;

struct LogicPosition {
    int x=0,y=0,z=0;
    LogicPosition()=default;
    LogicPosition(glm::ivec3 p):x(p.x),y(p.y),z(p.z){}
    operator glm::ivec3() const {return {x,y,z};}
    bool operator<(const LogicPosition& b) const {return x!=b.x?x<b.x:y!=b.y?y<b.y:z<b.z;}
    bool operator==(const LogicPosition& b)const{return x==b.x&&y==b.y&&z==b.z;}
};
bool automationBlock(Block);
bool automationDevice(Block); // Includes the existing buttons, plates and powered doors.
int automationSignal(Block);
glm::ivec3 automationFacing(Block);
bool validAutomationState(Block);
const char* automationDescription(Block);

// Main-thread owned. No terrain generation, worker callbacks or background writes.
class Automation {
    friend class EngineOptimizationTest;
    friend class AutomationTest;
public:
    struct Node {
        Block type=Block::Air;
        std::uint16_t remaining=0;
        std::uint8_t target=0, input=0;
        Block observed=Block::Air;
        std::uint64_t due=0,revision=1,timerAt=0;
    };
    struct Diagnostics {
        std::size_t devices=0,dirty=0,processed=0,changes=0,transfers=0,moves=0;
        std::uint64_t ticks=0,overloadTicks=0;
        float milliseconds=0;
        float tickMilliseconds=0,preparationMilliseconds=0,evaluationMilliseconds=0,commitMilliseconds=0;
        unsigned reconciled=0,dueProcessed=0;
    };
    void changed(World&,glm::ivec3,Block previous,Block next);
    void activateChunk(World&,int x,int z);
    void suspendChunk(int x,int z);
    void update(float dt,World&,SurvivalWorld&,const Player*,float daylight,SoundSystem* sounds=nullptr);
    bool interact(World&,glm::ivec3);
    bool valid() const;
    bool canRegister(glm::ivec3 p,Block b)const{return !automationDevice(b)||nodes_.count(p)||nodes_.size()<100000;}
    bool canRegisterPlan(const std::vector<std::pair<glm::ivec3,Block>>&) const;
    bool placementsValid(const std::function<Block(glm::ivec3)>& savedBlock) const;
    bool read(std::istream&,int minimumY,int maximumY);
    bool write(std::ostream&) const;
    std::function<bool(std::ostream&)> captureWrite() const;
    const Diagnostics& diagnostics()const{return stats_;}
    void reset();
    std::function<bool(const glm::vec3&,const glm::vec3&)> additionalCollision;
private:
    std::map<LogicPosition,Node> nodes_;
    std::map<std::pair<int,int>,std::set<LogicPosition>> chunks_;
    std::set<LogicPosition> active_,dirty_;
    std::map<std::pair<std::uint64_t,LogicPosition>,bool> scheduled_;
    std::set<LogicPosition> wave_;
    struct Change {LogicPosition position;Block before,after;std::uint64_t revision;int electricalInput=-1;};
    std::vector<Change> changes_;
    std::size_t cursor_=0;
    double accumulator_=0;
    Diagnostics stats_;
    bool applying_=false;
    std::uint64_t revision_=1;
    std::size_t observerCount_=0;
    struct RangeSample {std::uint64_t tick=0;int x=0,z=0,radius=0;bool valid=false;};
    std::array<RangeSample,128> ranges_{};
    RangeSample lastRange_{};
    LogicPosition reconcileCursor_;
    bool reconcilePending_=true,reconcileAgain_=false,reconcileStarted_=false;
    unsigned remainingTicks(LogicPosition,const Node&) const;
    void schedule(LogicPosition,unsigned ticks);
    void wake(glm::ivec3);
    void pause(LogicPosition);
    bool ready(const World&,glm::ivec3)const;
    int input(const World&,glm::ivec3,int direction=-1)const;
    void tick(World&,SurvivalWorld&,const Player*,float,SoundSystem*);
    bool transfer(World&,glm::ivec3,Block);
    bool piston(World&,SurvivalWorld&,const Player*,glm::ivec3,Block,bool);
    bool harvest(World&,glm::ivec3,Block);
};
