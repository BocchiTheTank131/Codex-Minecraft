#pragma once
#include "Survival.h"
#include <map>
#include <array>
#include <optional>
#include <istream>
#include <ostream>
struct StructurePlan;
class World; class Player;
enum class VillageJob : std::uint8_t { Farmer, Smith, Librarian, Merchant };
struct VillageOffer {Item input;int cost;Item output;int count;};
struct VillageRequest {std::uint8_t kind=0,status=0;std::uint16_t progress=0;};
struct VillageRecord {
 std::uint64_t id=0,seconds=0,rotatedAt=0,threatUntil=0,penaltyAt=0;
 int regionX=0,regionZ=0;glm::ivec3 center{0};
 std::int16_t reputation=0;std::uint16_t supply=16,rotation=0;
 std::uint8_t tradeReputation=0,projects=0;
 std::uint32_t completedRequests=0;
 std::array<std::array<std::uint16_t,2>,2> funding{};
 std::array<glm::ivec3,2> projectOrigin{};
 std::array<VillageRequest,3> requests{};
};
struct VillageNpc {
 std::uint32_t id=0,trades=0;std::uint64_t village=0,restockAt=300;
 glm::ivec3 home{0},work{0};VillageJob job=VillageJob::Merchant;
 std::array<std::uint8_t,5> stock{{4,4,4,4,2}};
 std::uint8_t routine=0,variant=0;bool dead=false;
};
class Villages {
public:
 static constexpr unsigned MaxVillages=512,MaxNpcs=8192;
 static std::uint64_t identity(std::uint32_t seed,unsigned version,int rx,int rz,glm::ivec3 center);
 static const char* profession(VillageJob);
 static const char* reputationName(int);
 static const char* routineName(unsigned);
 static VillageOffer offer(VillageJob,int row);
 VillageOffer pricedOffer(std::uint32_t,int)const;
 static bool exchange(Inventory&,Item input,int count,ItemStack output,std::string&);
 std::uint32_t bind(World&,glm::vec3 home);
 VillageNpc* npc(std::uint32_t);const VillageNpc* npc(std::uint32_t)const;
 VillageRecord* village(std::uint64_t);const VillageRecord* village(std::uint64_t)const;
 const auto& records()const{return villages_;}const auto& people()const{return npcs_;}
 void advance(float,World&);
 glm::vec3 destination(std::uint32_t,float time,bool shelter,bool danger);
 bool trade(std::uint32_t,int,Inventory&,std::string&);
 bool request(std::uint32_t,int,int action,Inventory&,Player&,std::string&);
 bool contribute(std::uint32_t,int,Inventory&,std::string&);
 bool withdraw(std::uint32_t,int,Inventory&,std::string&);
 std::string projectSite(std::uint32_t,int,const World&)const;
 bool construct(std::uint32_t,int,World&,SurvivalWorld&,Player&,std::string&);
 void attacked(std::uint32_t,bool killed);
 void assistance(glm::vec3);
 void damageProperty(glm::ivec3);
 std::vector<std::string> lines(std::uint32_t,int tab)const;
 bool write(std::ostream&,std::uint32_t seed,unsigned version)const;
 bool read(std::istream&,std::uint32_t seed,unsigned version);
 bool valid(std::uint32_t seed,unsigned version)const;
private:
 std::map<std::uint64_t,VillageRecord> villages_;
 std::map<std::uint32_t,VillageNpc> npcs_;
 float accumulator_=0;
 void rotate(VillageRecord&);
};
