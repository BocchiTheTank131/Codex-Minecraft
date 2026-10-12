#pragma once
#include "Structure.h"
#include "Biome.h"

enum class UndergroundKind : std::uint8_t { Mineshaft, Dungeon, AncientRuins };
struct UndergroundSite { int height; Biome biome; float river; };
struct UndergroundRoom { int x,y,z,halfX,halfZ,height; };
struct UndergroundPlan {
    UndergroundKind kind=UndergroundKind::Mineshaft;
    int regionX=0,regionZ=0;
    StructureBox bounds{};
    Biome biome=Biome::Plains;
    std::vector<UndergroundRoom> rooms;
    // Canonical world-coordinate cells, unique and sorted. Chunk placement is clipped.
    std::vector<StructureCell> cells;
    std::vector<StructureLootMarker> loot;
    std::vector<StructureMobMarker> mobs;
    StructureConnector entrance{};
};
class UndergroundStructures {
public:
    static constexpr int RegionSize=256;
    explicit UndergroundStructures(std::uint32_t seed, unsigned version=19):seed_(seed),version_(version){}
    using Site=std::function<UndergroundSite(int,int)>;
    using Cave=std::function<bool(int,int,int)>; // dry, two-block opening with ground
    using Reserved=std::function<bool(const StructureBox&)>;
    std::shared_ptr<const UndergroundPlan> plan(int rx,int rz,const Site&,const Cave&,const Reserved&) const;
    bool apply(int cx,int cz,std::vector<Block>&,std::vector<StructureLootMarker>&,
               std::vector<StructureMobMarker>&,const Site&,const Cave&,const Reserved&,int minY,int maxY) const;
    static const char* name(UndergroundKind);
private:
    std::uint32_t seed_;
    unsigned version_;
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::int64_t,std::shared_ptr<const UndergroundPlan>> cache_;
    mutable std::deque<std::int64_t> order_;
    std::shared_ptr<const UndergroundPlan> build(int rx,int rz,const Site&,const Cave&,const Reserved&) const;
};
