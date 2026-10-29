#pragma once
#include "Survival.h"
#include "Biome.h"
#include <map>
#include <istream>
#include <ostream>
#include <optional>

inline int adventureFloorDiv(int n,int d){return n/d-(n%d<0);}
class World;
class Player;
struct UndergroundPlan;
enum class RelicPuzzle : std::uint8_t { Sequence, Routing, Plates };
struct RelicChamber {
    int regionX=0,regionZ=0;
    glm::ivec3 center{0}; // floor centre; all offsets are world aligned
    RelicPuzzle kind=RelicPuzzle::Sequence;
    glm::ivec3 control(int n) const {return center+glm::ivec3(-3+n*3,1,-3);}
    glm::ivec3 gate() const {return center+glm::ivec3(2,1,4);}
    glm::ivec3 chest() const {return center+glm::ivec3(5,1,4);}
};
struct AdventureDiscovery {
    std::uint8_t kind=0; // 0..6 loot sites, 7 cave, 8 V24 chamber
    glm::ivec3 position{0};
};
struct RelicProgress {
    glm::ivec3 center{0};
    RelicPuzzle kind=RelicPuzzle::Sequence;
    std::uint8_t sequence=0,flags=0; // solved, trial started, trial won, reward acknowledged
    std::uint16_t trialTicks=0;
};
class Adventure {
public:
    static constexpr std::size_t MaxDiscoveries=4096, MaxPuzzles=2048;
    static std::uint64_t regionId(int x,int z);
    bool discover(std::uint8_t kind,glm::ivec3 position);
    void observeInventory(const Inventory&);
    void biome(Biome);
    bool useControl(World&,const RelicChamber&,glm::ivec3,std::string&);
    bool useTreasure(World&,const RelicChamber&,Inventory&,Player&,bool creative,std::string&);
    bool locked(const RelicChamber&) const;
    void update(float,World&,SurvivalWorld&,Player&,Inventory&);
    bool useTool(World&,Player&,Inventory&,bool offhand,bool creative,std::string&,bool echo=false,bool* newSurvey=nullptr);
    std::vector<std::string> journal(int category,int page) const;
    std::string takeNotice();
    bool write(std::ostream&) const;
    bool read(std::istream&);
    const std::vector<AdventureDiscovery>& discoveries() const {return discoveries_;}
    const std::map<std::uint64_t,RelicProgress>& puzzles() const {return puzzles_;}
    std::optional<glm::ivec3> surveyTarget;
    std::uint64_t biomes=0;
    std::uint32_t relics=0,crafted=0,achievements=0;
private:
    unsigned discoveryTick_=0;
    float accumulator_=0;
    std::uint16_t compassCooldown_=0,lanternCooldown_=0;
    std::vector<AdventureDiscovery> discoveries_;
    std::map<std::uint64_t,RelicProgress> puzzles_;
    std::string notice_;
    RelicProgress* progress(const RelicChamber&);
    void notify(std::string);
};
bool isRelicTool(Item);
bool isCollectibleRelic(Item);
bool validRelicEncounterTag(std::uint64_t);
const char* relicPuzzleName(RelicPuzzle);
// Mutates only a newly built V24 immutable plan, before chunk clipping.
void addRelicChamber(UndergroundPlan&,std::uint32_t seed);
RelicChamber relicChamber(const UndergroundPlan&,std::uint32_t seed);
