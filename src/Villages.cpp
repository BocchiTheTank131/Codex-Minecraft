#include "Villages.h"
#include "World.h"
#include "Player.h"
#include "SaveValidation.h"
#include "WorldTime.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <limits>
#include <cstring>
namespace {
constexpr std::uint64_t MaxClock=1000000000000ULL;
bool pos(glm::ivec3 p){return p.x>-30000000&&p.x<30000000&&p.z>-30000000&&p.z<30000000&&p.y>=-64&&p.y<320;}
template<class T>void put(std::ostream& o,const T& v){o.write(reinterpret_cast<const char*>(&v),sizeof(v));}
template<class T>bool get(std::istream& i,T& v){return bool(i.read(reinterpret_cast<char*>(&v),sizeof(v)));}
void point(std::ostream& o,glm::ivec3 p){put(o,p.x);put(o,p.y);put(o,p.z);}
bool point(std::istream& i,glm::ivec3& p){return get(i,p.x)&&get(i,p.y)&&get(i,p.z)&&pos(p);}
std::array<int,2> costs(int project){return project?std::array<int,2>{8,4}:std::array<int,2>{16,4};}
std::array<Item,2> materials(int project){return project?std::array<Item,2>{Item::Log,Item::IronIngot}:std::array<Item,2>{Item::Cobblestone,Item::Log};}
int available(const Inventory& inv,Item item){int n=0;for(int i=0;i<Inventory::TotalSlots;++i){const auto& s=inv.slot(i);if(!inv.hotbarLocked(i)&&s.item==item&&!s.identity&&!s.durability)n+=s.count;}return n;}
VillageOffer objective(unsigned kind){switch(kind){case 0:return {Item::Bread,12,Item::IronIngot,2};case 1:return {Item::Seeds,24,Item::Bread,4};case 2:return {Item::Cobblestone,16,Item::Torch,8};default:return {Item::None,0,Item::CookedBeef,4};}}
}
std::uint64_t Villages::identity(std::uint32_t seed,unsigned version,int rx,int rz,glm::ivec3 c){std::uint64_t h=1469598103934665603ULL;for(auto v:{seed,std::uint32_t(version),std::uint32_t(rx),std::uint32_t(rz),std::uint32_t(c.x),std::uint32_t(c.y),std::uint32_t(c.z)})for(unsigned b=0;b<4;++b){h^=(v>>(8*b))&255;h*=1099511628211ULL;}return h?h:1;}
const char* Villages::profession(VillageJob j){static const char* names[]={"FARMER","BLACKSMITH","LIBRARIAN","MERCHANT"};return unsigned(j)<4?names[unsigned(j)]:"?";}
const char* Villages::reputationName(int n){return n<0?"UNTRUSTED":n<20?"NEUTRAL":n<45?"FRIENDLY":n<75?"TRUSTED":"HONORED";}
const char* Villages::routineName(unsigned n){static const char* names[]={"WORK / TRAVEL","MEETING / TRAVEL","REST / RETURN HOME","SEEKING SHELTER","FLEEING"};return n<5?names[n]:"?";}
VillageOffer Villages::offer(VillageJob j,int row){static const VillageOffer offers[4][5]={
 {{Item::Wheat,20,Item::Bread,4},{Item::Carrot,16,Item::Potato,8},{Item::Egg,8,Item::Seeds,4},{Item::IronIngot,2,Item::CookedBeef,5},{Item::GoldIngot,1,Item::PumpkinSeeds,4}},
 {{Item::IronIngot,6,Item::IronPickaxe,1},{Item::Coal,16,Item::IronIngot,2},{Item::IronIngot,4,Item::IronShovel,1},{Item::CopperIngot,12,Item::IronIngot,2},{Item::GoldIngot,6,Item::Diamond,1}},
 {{Item::Paper,24,Item::Book,2},{Item::Book,4,Item::EmptyMap,1},{Item::GoldIngot,3,Item::SurveyorToken,1},{Item::Paper,32,Item::Torch,8},{Item::GoldIngot,4,Item::CavernShard,1}},
 {{Item::Cobblestone,48,Item::Log,8},{Item::Log,16,Item::CookedBeef,4},{Item::IronIngot,2,Item::Coal,4},{Item::IronIngot,3,Item::OakBoat,1},{Item::GoldIngot,2,Item::EmptyMap,1}}};return offers[std::min(3u,unsigned(j))][std::clamp(row,0,4)];}
bool Villages::exchange(Inventory& inv,Item input,int count,ItemStack output,std::string& msg){
 if(count<0||count>64||(!output.empty()&&!SaveValidation::stack(output))){msg="Invalid transaction";return false;}
 Inventory copy=inv;int left=count;
 for(int i=0;i<Inventory::TotalSlots&&left;++i){auto& s=copy.slots_[i];if(copy.hotbarLocked(i)||s.item!=input||s.identity||s.durability)continue;int n=std::min(left,s.count);s.count-=n;left-=n;if(!s.count)s.clear();}
 if(left){msg="Missing "+std::to_string(left)+" "+itemDefinition(input).displayName+" / unlocked inventory only";return false;}
 for(int pass=0;pass<2&&!output.empty();++pass)for(int i=0;i<Inventory::TotalSlots&&!output.empty();++i){auto& s=copy.slots_[i];if(copy.hotbarLocked(i))continue;if((pass==0&&!s.empty()&&Inventory::canMerge(s,output))||(pass==1&&s.empty())){int n=std::min(output.count,Inventory::maxStack(output.item)-(s.empty()?0:s.count));if(s.empty()){s=output;s.count=n;}else s.count+=n;output.count-=n;if(!output.count)output.clear();}}
 if(!output.empty()){msg="Inventory full / protected slots are not used";return false;}inv=std::move(copy);return true;
}
VillageNpc* Villages::npc(std::uint32_t id){auto i=npcs_.find(id);return i==npcs_.end()?nullptr:&i->second;}
const VillageNpc* Villages::npc(std::uint32_t id)const{auto i=npcs_.find(id);return i==npcs_.end()?nullptr:&i->second;}
VillageRecord* Villages::village(std::uint64_t id){auto i=villages_.find(id);return i==villages_.end()?nullptr:&i->second;}
const VillageRecord* Villages::village(std::uint64_t id)const{auto i=villages_.find(id);return i==villages_.end()?nullptr:&i->second;}
void Villages::rotate(VillageRecord& v){for(unsigned i=0;i<3;++i){unsigned kind=(v.id+v.rotation+i)%4;if(kind==2&&(v.projects&1))kind=0;v.requests[i]={std::uint8_t(kind),0,0};}}
std::uint32_t Villages::bind(World& w,glm::vec3 home){
 if(npcs_.size()>=MaxNpcs||!SaveValidation::position(home))return 0;
 VillageNpc n;n.id=std::uint32_t(npcs_.size()+1);n.home=glm::ivec3(glm::floor(home));n.work=n.home;if(!pos(n.home))return 0;
 auto plan=w.cachedVillageNear(n.home);
 if(plan){const auto& meeting=plan->pieces.front();auto c=glm::ivec3(meeting.x,meeting.y+1,meeting.z);const auto id=identity(w.seed(),w.generationVersion(),plan->regionX,plan->regionZ,c);
  unsigned population=0;for(const auto& e:npcs_)population+=e.second.village==id;
  if(population<64&&(villages_.count(id)||villages_.size()<MaxVillages)){
   auto it=villages_.find(id);if(it==villages_.end()){VillageRecord v;v.id=id;v.regionX=plan->regionX;v.regionZ=plan->regionZ;v.center=c;rotate(v);it=villages_.emplace(id,v).first;}
   n.village=id;n.restockAt=it->second.seconds+300;
   auto hash=identity(w.seed(),w.generationVersion(),n.home.x,n.home.z,n.home);n.job=VillageJob(hash%4);n.variant=(hash>>4)&1;
   StructurePieceKind want=n.job==VillageJob::Farmer?StructurePieceKind::Farm:n.job==VillageJob::Smith?StructurePieceKind::Smith:n.job==VillageJob::Librarian?StructurePieceKind::Storage:StructurePieceKind::Well;
   auto nearest=[&](StructurePieceKind kind){auto best=plan->pieces.end();float score=std::numeric_limits<float>::max();for(auto i=plan->pieces.begin();i!=plan->pieces.end();++i)if(i->kind==kind){float d=glm::length(glm::vec2(i->x-n.home.x,i->z-n.home.z));if(d<score){score=d;best=i;}}return best;};
   auto chosen=nearest(want);if(chosen==plan->pieces.end()){n.job=VillageJob::Merchant;chosen=nearest(StructurePieceKind::Well);}
   if(chosen!=plan->pieces.end()){float best=std::numeric_limits<float>::max();for(const auto& door:structureTemplate(chosen->kind).connectors){auto d=rotateLocalPosition(door.x,door.z,chosen->rotation,chosen->mirror);glm::ivec3 p(chosen->x+d[0],chosen->y+door.y+1,chosen->z+d[1]);float distance=glm::length(glm::vec2(p.x-n.home.x,p.z-n.home.z));if(distance<best){best=distance;n.work=p;}}}
   if(chosen!=plan->pieces.end()&&n.job==VillageJob::Farmer){float best=std::numeric_limits<float>::max();for(const auto& cell:structureTemplate(chosen->kind).cells)if(cell.operation==TemplateOperation::Place&&cell.block==Block::Farmland){auto d=rotateLocalPosition(cell.x,cell.z,chosen->rotation,chosen->mirror);glm::ivec3 p(chosen->x+d[0],chosen->y+cell.y+1,chosen->z+d[1]);float distance=glm::length(glm::vec2(p.x-n.home.x,p.z-n.home.z));if(distance<best){best=distance;n.work=p;}}}
  }
 }
 npcs_.emplace(n.id,n);return n.id;
}
void Villages::advance(float dt,World& w){if(!std::isfinite(dt)||dt<=0)return;accumulator_+=dt;if(accumulator_<1)return;accumulator_=std::fmod(accumulator_,1.f);
 for(auto& e:villages_){auto& v=e.second;if(v.seconds>=MaxClock-601||!w.hasLoadedChunkAt(v.center.x,v.center.z)||!w.simulationActiveAt(float(v.center.x),float(v.center.z)))continue;++v.seconds;}
}
glm::vec3 Villages::destination(std::uint32_t id,float time,bool shelter,bool danger){auto n=npc(id);if(!n)return {};auto v=village(n->village);
 const float t=WorldTime::wrap(time);n->routine=danger?4:shelter?3:t>=700?2:t>=480?1:0;
 if(v&&v->seconds>=n->restockAt){if(v->supply>=4){for(auto& s:n->stock)s=std::min(6,int(s)+1);v->supply-=4;}n->restockAt=v->seconds+300;}
 if(danger&&v)v->threatUntil=std::min(MaxClock,v->seconds+20);
 if(n->routine>=2)return glm::vec3(n->home)+glm::vec3(.5f,.01f,.5f);
 auto target=n->routine==1&&v?v->center+buildingFacing(int(n->id%4))*5:n->work;return glm::vec3(target)+glm::vec3(.5f,.01f,.5f);
}
bool Villages::trade(std::uint32_t id,int row,Inventory& inv,std::string& msg){auto n=npc(id);if(!n||n->dead||row<0||row>=5){msg="Villager unavailable";return false;}auto v=village(n->village);if(row==4&&(!v||!(v->projects&2))){msg="Build the community stall to unlock this offer";return false;}if(n->trades>=1000000000){msg="Lifetime trade limit reached";return false;}if(n->routine>=2){msg="Resting, sheltering or fleeing; return during work hours";return false;}if(!n->stock[row]){msg="Out of stock / replenishment needs village supplies and active time";return false;}
 auto o=pricedOffer(id,row);
 if(!exchange(inv,o.input,o.cost,{o.output,o.count,Inventory::maxDurability(o.output)},msg))return false;
 --n->stock[row];++n->trades;if(v){v->supply=std::min(256,int(v->supply)+2);if(v->tradeReputation<10){++v->tradeReputation;v->reputation=std::min(100,int(v->reputation)+1);}}
 msg="Trade complete / stock updated";return true;
}
bool Villages::request(std::uint32_t id,int row,int action,Inventory& inv,Player& p,std::string& msg){auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v||n->dead||row<0||row>=3||action<0||action>3){msg="No village request available";return false;}
 auto& q=v->requests[row];if(action==0){if(q.status!=0){msg="Request already accepted or finished";return false;}q.status=1;msg="Request accepted / objective tracked here";return true;}
 if(action==2){if(q.status!=1){msg="No accepted request";return false;}q.status=3;msg="Request abandoned; donated project funding is retained";return true;}
 if(action==3){if(std::any_of(v->requests.begin(),v->requests.end(),[](auto a){return a.status<2;})||v->seconds<v->rotatedAt+600||v->rotation>=10000){msg="Finish or abandon this set; rotation requires 600 active seconds";return false;}++v->rotation;v->rotatedAt=v->seconds;rotate(*v);msg="New request set posted";return true;}
 if(q.status!=1){msg="Accept this request first; completed rewards cannot repeat";return false;}
 auto o=objective(q.kind);if(q.kind==3&&!q.progress){msg="Help during a witnessed village threat: defeat a nearby hostile";return false;}
 if(q.kind==2&&!(v->projects&1)){msg="Complete funded Village Repairs, then claim this request";return false;}
 if(!exchange(inv,q.kind==2?Item::None:o.input,q.kind==2?0:o.cost,{o.output,o.count,Inventory::maxDurability(o.output)},msg))return false;
 q.status=2;++v->completedRequests;v->reputation=std::min(100,int(v->reputation)+8);v->supply=std::min(256,int(v->supply)+8);p.addExperience(4);msg="Request complete / +8 reputation / +4 XP";return true;
}
bool Villages::contribute(std::uint32_t id,int project,Inventory& inv,std::string& msg){auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v||n->dead||project<0||project>1||(v->projects&(1<<project))){msg="Project unavailable or already complete";return false;}auto cost=costs(project);auto mat=materials(project);Inventory copy=inv;auto funding=v->funding[project];bool changed=false;
 for(int i=0;i<2;++i){int amount=std::min(cost[i]-funding[i],available(copy,mat[i]));if(amount>0){if(!exchange(copy,mat[i],amount,{},msg))return false;funding[i]+=amount;changed=true;}}
 if(!changed){msg="No needed materials in unlocked slots / project may be fully funded";return false;}
 inv=std::move(copy);v->funding[project]=funding;msg="Contribution retained / confirm construction separately";return true;
}
void Villages::attacked(std::uint32_t id,bool killed){auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v)return;if(killed||v->seconds>=v->penaltyAt){v->reputation=std::max(-100,int(v->reputation)-(killed?20:5));v->penaltyAt=v->seconds+2;}if(killed)n->dead=true;}
void Villages::assistance(glm::vec3 p){for(auto& e:villages_){auto& v=e.second;if(v.threatUntil<=v.seconds||glm::length(glm::vec2(p.x-v.center.x,p.z-v.center.z))>72)continue;for(auto& q:v.requests)if(q.kind==3&&q.status==1)q.progress=1;}}

VillageOffer Villages::pricedOffer(std::uint32_t id,int row)const{auto n=npc(id);auto o=offer(n?n->job:VillageJob::Merchant,row);if(n&&n->variant&&n->job==VillageJob::Farmer&&row==1){o.output=Item::Beetroot;o.count=6;}auto v=n?village(n->village):nullptr;if(v&&o.cost>=8){if(v->reputation>=20)o.cost-=v->reputation>=75?2:1;else if(v->reputation<0)o.cost+=2;}return o;}
namespace {
std::vector<std::pair<glm::ivec3,Block>> projectCells(const VillageRecord& v,int project,const World& w){
 auto plan=w.cachedVillageNear(v.center);if(!plan||plan->regionX!=v.regionX||plan->regionZ!=v.regionZ)return {};
 std::vector<std::pair<glm::ivec3,Block>> edits;
 if(!project){for(const auto& road:plan->roads){glm::ivec3 p(road.x,road.y,road.z);if(!w.hasLoadedChunkAt(p.x,p.z))return {};auto old=w.getBlock(p.x,p.y,p.z);if(old!=Block::Air&&(old!=road.block||!w.villageSiteUnmodified(p)))continue;edits.push_back({p,Block::StoneBricks});if(edits.size()==4)return edits;}return {};}
 // A small explicit 2x2 stall, outside all canonical building footprints.
 for(auto d:{glm::ivec2(3,8),glm::ivec2(-4,8),glm::ivec2(3,-9),glm::ivec2(-4,-9),glm::ivec2(8,3),glm::ivec2(8,-4),glm::ivec2(-9,3),glm::ivec2(-9,-4),glm::ivec2(7,7),glm::ivec2(-8,7),glm::ivec2(7,-8),glm::ivec2(-8,-8)}){
  glm::ivec3 o=v.center+glm::ivec3(d.x,-1,d.y);bool clear=true;
  for(const auto& piece:plan->pieces)if(piece.bounds.intersectsXZ(o.x-1,o.z-1,o.x+2,o.z+2))clear=false;
  for(int z=0;z<2;++z)for(int x=0;x<2;++x){if(!w.hasLoadedChunkAt(o.x+x,o.z+z)){clear=false;continue;}auto ground=w.getBlock(o.x+x,o.y,o.z+z);if((ground!=Block::Grass&&ground!=Block::Dirt&&ground!=Block::Sand)||!w.villageSiteUnmodified(o+glm::ivec3(x,0,z)))clear=false;
   for(int y=1;y<4;++y)if(w.getBlock(o.x+x,o.y+y,o.z+z)!=Block::Air)clear=false;}
  if(!clear)continue;
  edits.clear();for(int z=0;z<2;++z)for(int x=0;x<2;++x){edits.push_back({o+glm::ivec3(x,0,z),Block::Planks});edits.push_back({o+glm::ivec3(x,3,z),Block::Planks});}
  for(int x=0;x<2;++x)for(int y=1;y<3;++y)edits.push_back({o+glm::ivec3(x,y,0),Block::OakFence});edits.push_back({o+glm::ivec3(1,1,1),Block::Barrel});return edits;
 }return {};
}
}
bool Villages::withdraw(std::uint32_t id,int project,Inventory& inv,std::string& msg){auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v||n->dead||project<0||project>1||(v->projects&(1<<project))){msg="Completed or unavailable project cannot refund";return false;}Inventory copy=inv;auto m=materials(project);bool any=false;for(int i=0;i<2;++i)if(v->funding[project][i]){any=true;if(!exchange(copy,Item::None,0,{m[i],v->funding[project][i],0},msg))return false;}if(!any){msg="No pledged materials to refund";return false;}inv=std::move(copy);v->funding[project]={0,0};msg="Unspent materials refunded / no reward or reputation granted";return true;}
std::string Villages::projectSite(std::uint32_t id,int project,const World& w)const{auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v||project<0||project>1)return "No associated village";auto edits=projectCells(*v,project,w);auto p=v->projects&(1<<project)?std::optional<glm::ivec3>(v->projectOrigin[project]):edits.empty()?std::optional<glm::ivec3>{}:edits.front().first;if(!p)return "SITE BLOCKED OR UNLOADED / FUNDING IS REFUNDABLE";return std::string(v->projects&(1<<project)?"COMPLETED SITE ":"PROPOSED SITE ")+std::to_string(p->x)+" "+std::to_string(p->y)+" "+std::to_string(p->z);}
bool Villages::construct(std::uint32_t id,int project,World& w,SurvivalWorld& mobs,Player& player,std::string& msg){auto n=npc(id);auto v=n?village(n->village):nullptr;if(!v||n->dead||project<0||project>1||(v->projects&(1<<project))){msg="Already complete or no associated village";return false;}
 auto cost=costs(project);for(int i=0;i<2;++i)if(v->funding[project][i]!=cost[i]){msg="Project needs full funding / contributions retained";return false;}
 auto edits=projectCells(*v,project,w);if(edits.empty()){msg="No loaded, clear approved site / funding retained";return false;}
 std::vector<std::pair<glm::ivec3,Block>> before;for(auto e:edits){auto p=e.first;if(!w.hasLoadedChunkAt(p.x,p.z)||!w.blockEntityContents(p).empty()){msg="Owned or unloaded site / funding retained";return false;}auto low=glm::vec3(p),high=low+glm::vec3(1);
  auto overlap=[](glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d){return a.x<d.x&&b.x>c.x&&a.y<d.y&&b.y>c.y&&a.z<d.z&&b.z>c.z;};
  if((!player.isSpectator()&&overlap(low,high,player.aabbMinimum(),player.aabbMaximum()))||mobs.anyMobIntersects(low,high)){msg="Player or villager in the way / funding retained";return false;}before.push_back({p,w.getBlock(p.x,p.y,p.z)});}
 if(!w.automationBatch(before,edits)){msg="Construction rejected / funding retained";return false;}
 v->projects|=1<<project;v->projectOrigin[project]=edits.front().first;v->reputation=std::min(100,int(v->reputation)+12);v->supply=std::min(256,int(v->supply)+16);player.addExperience(6);msg="Construction complete / +12 reputation / +6 XP";return true;
}
void Villages::damageProperty(glm::ivec3 p){for(auto& e:villages_){auto& v=e.second;bool owned=false;for(int i=0;i<2;++i)if(v.projects&(1<<i)){auto o=v.projectOrigin[i];owned|=i? p.x>=o.x&&p.x<=o.x+1&&p.z>=o.z&&p.z<=o.z+1&&p.y>=o.y&&p.y<=o.y+3:p==o;}
 if(owned&&v.seconds>=v.penaltyAt){v.reputation=std::max(-100,int(v.reputation)-3);v.penaltyAt=v.seconds+5;}}
}
std::vector<std::string> Villages::lines(std::uint32_t id,int tab)const{auto n=npc(id);if(!n)return {"Villager unavailable"};auto v=village(n->village);std::vector<std::string> out;
 if(tab==0){for(int i=0;i<5;++i){auto o=pricedOffer(id,i);out.push_back(std::to_string(o.cost)+" "+itemDefinition(o.input).displayName+" -> "+std::to_string(o.count)+" "+itemDefinition(o.output).displayName+" / STOCK "+std::to_string(n->stock[i])+(i==4&&(!v||!(v->projects&2))?" / NEEDS STALL":""));}}
 else if(!v)out={"Independent traveler / no village requests or projects","Find a generated village to join community activities."};
 else if(tab==1){for(auto q:v->requests){auto o=objective(q.kind);const char* names[]={"DELIVERY","FARMING / SEED SUPPLIES","BUILDING / REPAIRS","ASSISTANCE / WITNESSED THREAT"};const char* states[]={"OFFERED","ACCEPTED","CLAIMED","ABANDONED"};out.push_back(std::string(names[q.kind])+" / "+states[q.status]);out.push_back(q.kind==2?"Finish Village Repairs / reward 8 torches":q.kind==3?std::string(q.progress?"Threat cleared / reward 4 cooked beef":"Help a threatened villager; defeat its local attacker / reward 4 cooked beef"):"Deliver "+std::to_string(o.cost)+" "+itemDefinition(o.input).displayName+" / reward "+std::to_string(o.count)+" "+itemDefinition(o.output).displayName);out.back()+=" / +8 REP / +4 XP";}}
 else if(tab==2){for(int i=0;i<2;++i){auto c=costs(i);auto m=materials(i);out.push_back(std::string(i?"COMMUNITY STALL":"VILLAGE REPAIRS")+(v->projects&(1<<i)?" / COMPLETE":" / FUNDING"));out.push_back(std::to_string(v->funding[i][0])+"/"+std::to_string(c[0])+" "+itemDefinition(m[0]).displayName+" + "+std::to_string(v->funding[i][1])+"/"+std::to_string(c[1])+" "+itemDefinition(m[1]).displayName);}}
 else {out={"VILLAGE "+std::to_string(v->regionX)+" / "+std::to_string(v->regionZ),"CENTER "+std::to_string(v->center.x)+" "+std::to_string(v->center.y)+" "+std::to_string(v->center.z),"SUPPLIES "+std::to_string(v->supply)+" / 256","Restock: one unit per offer per 300 ACTIVE seconds, costs 4 supplies.","Trade reputation: +1, lifetime cap 10. Requests: +8. Projects: +12.","Requests rotate after 600 active seconds and a finished set.","No automatic village rebuilding. Fund, then explicitly confirm."};}
 return out;
}
bool Villages::valid(std::uint32_t seed,unsigned version)const{if(villages_.size()>MaxVillages||npcs_.size()>MaxNpcs)return false;std::map<std::uint64_t,unsigned> populations;
 for(auto e:villages_){auto& v=e.second;if(e.first!=v.id||!pos(v.center)||v.id!=identity(seed,version,v.regionX,v.regionZ,v.center)||v.regionX!=adventureFloorDiv(v.center.x,320)||v.regionZ!=adventureFloorDiv(v.center.z,320)||v.seconds>MaxClock||v.rotatedAt>v.seconds||v.threatUntil>v.seconds+20||v.penaltyAt>v.seconds+5||v.rotation>10000||v.reputation < -100||v.reputation>100||v.supply>256||v.tradeReputation>10||v.projects>3||v.completedRequests>3u*(unsigned(v.rotation)+1))return false;
  for(int i=0;i<2;++i){auto cost=costs(i);for(int j=0;j<2;++j)if(v.funding[i][j]>cost[j]||((v.projects&(1<<i))&&v.funding[i][j]!=cost[j]))return false;if(!pos(v.projectOrigin[i]))return false;}
  for(auto q:v.requests)if(q.kind>3||q.status>3||q.progress>1||(q.kind!=3&&q.progress)||(q.status==2&&q.kind==2&&!(v.projects&1))||(q.status==2&&q.kind==3&&!q.progress))return false;
 }
 std::uint32_t expected=1;for(auto e:npcs_){auto& n=e.second;if(e.first!=expected++||n.id!=e.first||!pos(n.home)||!pos(n.work)||unsigned(n.job)>3||n.routine>4||n.variant>1||n.trades>1000000000)return false;for(auto stock:n.stock)if(stock>6)return false;if(n.village){auto v=village(n.village);if(!v||++populations[n.village]>64||n.restockAt>v->seconds+300||glm::length(glm::vec2(n.home.x-v->center.x,n.home.z-v->center.z))>160)return false;}else if(n.restockAt!=300)return false;}
 return true;
}
bool Villages::write(std::ostream& o,std::uint32_t seed,unsigned version)const{if(!valid(seed,version))return false;if(npcs_.empty()&&villages_.empty())return true;o.write("VLG1",4);put(o,std::uint32_t(villages_.size()));
 for(auto e:villages_){auto v=e.second;put(o,v.id);put(o,v.regionX);put(o,v.regionZ);point(o,v.center);put(o,v.seconds);put(o,v.rotatedAt);put(o,v.threatUntil);put(o,v.penaltyAt);put(o,v.reputation);put(o,v.supply);put(o,v.rotation);put(o,v.tradeReputation);put(o,v.projects);put(o,v.completedRequests);for(int i=0;i<2;++i){point(o,v.projectOrigin[i]);for(auto f:v.funding[i])put(o,f);}for(auto q:v.requests){put(o,q.kind);put(o,q.status);put(o,q.progress);}}
 put(o,std::uint32_t(npcs_.size()));for(auto e:npcs_){auto n=e.second;put(o,n.id);put(o,n.trades);put(o,n.village);point(o,n.home);point(o,n.work);put(o,n.job);put(o,n.restockAt);for(auto s:n.stock)put(o,s);put(o,n.routine);put(o,n.variant);put(o,std::uint8_t(n.dead));}return bool(o);
}
bool Villages::read(std::istream& in,std::uint32_t seed,unsigned version){Villages copy;char magic[4];std::uint32_t count=0;if(!in.read(magic,4)||std::memcmp(magic,"VLG1",4)||!get(in,count)||count>MaxVillages)return false;
 for(unsigned i=0;i<count;++i){VillageRecord v;if(!get(in,v.id)||!get(in,v.regionX)||!get(in,v.regionZ)||!point(in,v.center)||!get(in,v.seconds)||!get(in,v.rotatedAt)||!get(in,v.threatUntil)||!get(in,v.penaltyAt)||!get(in,v.reputation)||!get(in,v.supply)||!get(in,v.rotation)||!get(in,v.tradeReputation)||!get(in,v.projects)||!get(in,v.completedRequests))return false;
  for(int j=0;j<2;++j){if(!get(in,v.projectOrigin[j].x)||!get(in,v.projectOrigin[j].y)||!get(in,v.projectOrigin[j].z))return false;for(auto& f:v.funding[j])if(!get(in,f))return false;}for(auto& q:v.requests)if(!get(in,q.kind)||!get(in,q.status)||!get(in,q.progress))return false;if(!copy.villages_.emplace(v.id,v).second)return false;}
 if(!get(in,count)||count>MaxNpcs)return false;for(unsigned i=0;i<count;++i){VillageNpc n;std::uint8_t dead=0;if(!get(in,n.id)||!get(in,n.trades)||!get(in,n.village)||!point(in,n.home)||!point(in,n.work)||!get(in,n.job)||!get(in,n.restockAt))return false;for(auto& s:n.stock)if(!get(in,s))return false;if(!get(in,n.routine)||!get(in,n.variant)||!get(in,dead)||dead>1)return false;n.dead=dead!=0;if(!copy.npcs_.emplace(n.id,n).second)return false;}
 if(!copy.valid(seed,version))return false;*this=std::move(copy);return true;
}
