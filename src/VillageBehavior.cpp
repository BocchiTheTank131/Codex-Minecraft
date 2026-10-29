#include "Villages.h"
#include "World.h"
#include "Player.h"
#include <queue>
#include <set>
#include <cmath>
#include <limits>
namespace {
constexpr float VillagerWidth=.30f,VillagerHeight=1.60f;
bool villageLoaded(const World& w,glm::vec2 p){for(int z=int(std::floor(p.y-VillagerWidth));z<=int(std::floor(p.y+VillagerWidth));++z)for(int x=int(std::floor(p.x-VillagerWidth));x<=int(std::floor(p.x+VillagerWidth));++x)if(!w.hasLoadedChunkAt(x,z))return false;return true;}
bool villageClear(const World& w,glm::vec3 p){return villageLoaded(w,{p.x,p.z})&&!w.aabbIntersectsSolid(p-glm::vec3(VillagerWidth,0,VillagerWidth),p+glm::vec3(VillagerWidth,VillagerHeight,VillagerWidth));}
bool villageSweep(const World& w,glm::vec3& feet,int axis,float amount){
 if(amount==0)return false;constexpr float epsilon=.0001f;auto low=feet+glm::vec3(-VillagerWidth,0,-VillagerWidth),high=feet+glm::vec3(VillagerWidth,VillagerHeight,VillagerWidth);auto first=low,last=high;first[axis]+=std::min(amount,0.f);last[axis]+=std::max(amount,0.f);auto a=glm::ivec3(glm::floor(first+glm::vec3(epsilon))),b=glm::ivec3(glm::floor(last-glm::vec3(epsilon)));float allowed=amount;
 for(int z=a.z;z<=b.z;++z)for(int x=a.x;x<=b.x;++x)for(int y=std::max(w.minimumY(),a.y);y<=std::min(w.maximumY()-1,b.y);++y){glm::vec3 boxLow(x,y,z),boxHigh=boxLow+glm::vec3(1);if(w.hasLoadedChunkAt(x,z)&&!w.blockCollisionBounds({x,y,z},boxLow,boxHigh))continue;bool overlap=true;for(int other=0;other<3;++other)if(other!=axis&&(high[other]<=boxLow[other]+epsilon||low[other]>=boxHigh[other]-epsilon))overlap=false;if(!overlap)continue;if(amount>0&&high[axis]<=boxLow[axis]+epsilon)allowed=std::min(allowed,std::max(0.f,boxLow[axis]-high[axis]));else if(amount<0&&low[axis]>=boxHigh[axis]-epsilon)allowed=std::max(allowed,std::min(0.f,boxHigh[axis]-low[axis]));}
 feet[axis]+=allowed;return allowed!=amount;
}
}
bool SurvivalWorld::villageFooting(const World& w,glm::vec2 center,float height,float low,float high,float& result)const{
 if(!villageLoaded(w,center))return false;bool found=false;float best=-std::numeric_limits<float>::infinity();
 for(int y=int(std::floor(height+low))-1;y<=int(std::floor(height+high));++y){auto p=glm::ivec3(int(std::floor(center.x)),y,int(std::floor(center.y)));auto block=w.getBlock(p.x,p.y,p.z);auto shape=blockGeometry(block);if(!isSolid(block)||isWater(block)||isLava(block)||block==Block::Cactus||(shape.shape!=BlockShape::Cube&&shape.shape!=BlockShape::PartialCube)||blockDefinition(block).transparent)continue;glm::vec3 a,b;if(!w.blockCollisionBounds(p,a,b)||b.y<height+low-.02f||b.y>height+high+.02f||b.y<=best)continue;if(villageClear(w,{center.x,b.y+.01f,center.y})){found=true;best=b.y;}}
 if(found)result=best;return found;
}
void SurvivalWorld::moveVillager(Animal& a,World& w,glm::vec3 move,float dt,glm::vec3& next){
 next=a.position;int count=std::max(1,int(std::ceil(dt/.025f)));float step=dt/count;
 for(int i=0;i<count;++i){auto before=next;bool blockedX=villageSweep(w,next,0,move.x*step),blockedZ=villageSweep(w,next,2,move.z*step);
  if((blockedX||blockedZ)&&a.grounded&&a.stepJumpCooldown<=0&&glm::length(glm::vec2(move.x,move.z))>.01f){auto direction=glm::normalize(glm::vec2(move.x,move.z));float surface=0;if(villageFooting(w,glm::vec2(before.x,before.z)+direction*.95f,before.y,.08f,1.02f,surface)&&villageClear(w,before+glm::vec3(0,.1f,0))){a.velocity.y=6.2f;a.stepJumpCooldown=.7f;}}
  a.velocity.y-=18.f*step;bool falling=a.velocity.y<=0,blocked=villageSweep(w,next,1,a.velocity.y*step);a.grounded=falling&&blocked;if(blocked)a.velocity.y=0;
 }
}
std::shared_ptr<const StructurePlan> World::cachedVillageNear(glm::ivec3 home)const{
 std::shared_ptr<const StructurePlan> best;float distance=160;
 const int rx=floorDiv(home.x,320),rz=floorDiv(home.z,320);
 for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){auto p=structures_.peekMajorPlan(rx+x,rz+z);if(!p||p->pieces.empty()||(p->kind!=StructureKind::PlainsVillage&&p->kind!=StructureKind::DesertVillage))continue;auto c=p->pieces.front();auto d=glm::length(glm::vec2(home.x-c.x,home.z-c.z));if(d<distance&&home.x>=p->bounds.minX-12&&home.x<=p->bounds.maxX+12&&home.z>=p->bounds.minZ-12&&home.z<=p->bounds.maxZ+12){distance=d;best=p;}}
 return best;
}
std::uint32_t SurvivalWorld::villagerIdentity(int index,World& w){if(!isVillager(index))return 0;auto& a=animals_[index];if(!w.hasLoadedChunkAt(int(std::floor(a.position.x)),int(std::floor(a.position.z))))return 0;if(!a.villagerId)a.villagerId=w.villages.bind(w,a.home);return a.villagerId;}
bool SurvivalWorld::isVillager(int i)const{return i>=0&&i<int(animals_.size())&&animals_[i].type==AnimalType::Villager&&animals_[i].health>0&&animals_[i].deathTimer<=0;}
bool SurvivalWorld::villagerAvailable(std::uint32_t id,glm::vec3 p,float distance)const{if(!id)return false;auto it=villagerIndex_.find(id);if(it!=villagerIndex_.end()&&it->second<animals_.size()&&animals_[it->second].villagerId==id){const auto& a=animals_[it->second];return a.health>0&&a.deathTimer<=0&&glm::distance(a.position,p)<=distance;}for(std::size_t i=0;i<animals_.size();++i){const auto& a=animals_[i];if(a.villagerId==id){villagerIndex_[id]=i;return a.health>0&&a.deathTimer<=0&&glm::distance(a.position,p)<=distance;}}villagerIndex_.erase(id);return false;}
bool SurvivalWorld::villageCheckpoint(World& w,bool restore){std::set<std::uint32_t> ids;
 for(const auto& a:animals_)if(a.villagerId){auto n=w.villages.npc(a.villagerId);if(a.type!=AnimalType::Villager||!n||!ids.insert(a.villagerId).second||(restore&&n->dead&&a.health>0&&a.deathTimer<=0))return false;}
 for(auto& a:animals_)if(a.villagerId){auto n=w.villages.npc(a.villagerId);if(a.health<=0||a.deathTimer>0)n->dead=true;if(restore)a.home=glm::vec3(n->home)+glm::vec3(.5f,.01f,.5f);}
 return w.villages.valid(w.seed(),w.generationVersion());
}
glm::vec2 SurvivalWorld::villageHeading(Animal& a,World& w,glm::vec3 goal){
 // Route through the canonical home doorway before taking a straight course
 // to work. A locally closer wall is not an exit from a house.
 if(auto plan=w.cachedVillageNear(glm::ivec3(glm::floor(a.home)))){
  for(const auto& piece:plan->pieces){const auto& box=piece.bounds;
   if(a.home.x<box.minX||a.home.x>box.maxX+1||a.home.z<box.minZ||a.home.z>box.maxZ+1)continue;
   const auto& model=structureTemplate(piece.kind);if(model.mobs.empty()||model.connectors.empty())continue;
   const auto door=model.connectors.front();auto d=rotateLocalPosition(door.x,door.z,piece.rotation,piece.mirror);
   glm::vec3 entrance(piece.x+d[0]+.5f,piece.y+door.y+1.01f,piece.z+d[1]+.5f);
   glm::ivec2 wallMin(std::numeric_limits<int>::max()),wallMax(std::numeric_limits<int>::min());
   for(const auto& cell:model.cells)if(cell.operation==TemplateOperation::Place&&cell.y==1&&isSolid(cell.block)){auto r=rotateLocalPosition(cell.x,cell.z,piece.rotation,piece.mirror);auto p=glm::ivec2(piece.x+r[0],piece.z+r[1]);wallMin=glm::min(wallMin,p);wallMax=glm::max(wallMax,p);}
   if(a.home.x<wallMin.x||a.home.x>wallMax.x+1||a.home.z<wallMin.y||a.home.z>wallMax.y+1)continue;
   bool inside=a.position.x>=wallMin.x&&a.position.x<=wallMax.x+1&&a.position.z>=wallMin.y&&a.position.z<=wallMax.y+1;
   const bool returning=glm::distance(goal,a.home)<2;
   if((inside&&!returning)||(returning&&!inside&&glm::distance(a.position,entrance)>1.5f))goal=entrance;
   break;
  }
 }
 glm::vec2 delta(goal.x-a.position.x,goal.z-a.position.z);if(glm::dot(delta,delta)<.16f)return {};
 auto direct=glm::normalize(delta);if(glm::dot(delta,delta)<2.25f&&canNavigateTo(a,w,direct))return delta;
 // Open only a nearby ordinary door; never teleport or force a closed gate.
 auto feetCell=glm::ivec3(glm::floor(a.position));bool opened=false;
 for(int z=-1;z<=1&&!opened;++z)for(int x=-1;x<=1&&!opened;++x){auto ahead=feetCell+glm::ivec3(x,0,z);if(!w.hasLoadedChunkAt(ahead.x,ahead.z))continue;auto b=w.getBlock(ahead.x,ahead.y,ahead.z);
  if(isDoor(b)&&!isDoorOpen(b)){
   auto base=isDoorUpper(b)?ahead-glm::ivec3(0,1,0):ahead;auto bottom=w.getBlock(base.x,base.y,base.z),top=w.getBlock(base.x,base.y+1,base.z);
   if(isDoor(bottom)&&isDoor(top)&&isDoorUpper(top))opened=w.automationBatch({{base,bottom},{base+glm::ivec3(0,1,0),top}},{{base,doorBlock(doorFacing(bottom),false,doorHingeRight(bottom),true)},{base+glm::ivec3(0,1,0),doorBlock(doorFacing(top),true,doorHingeRight(top),true)}});
  }
 }
 if(opened&&glm::dot(delta,delta)<2.25f&&canNavigateTo(a,w,direct))return delta;
 if(!villageNavigationBudget_)return {};--villageNavigationBudget_;++villageQueriesLastTick_;
 struct Step{glm::ivec3 p;glm::vec2 first;int distance,order;float score;};
 auto compare=[](const Step& a,const Step& b){return a.score!=b.score?a.score>b.score:a.order>b.order;};std::priority_queue<Step,std::vector<Step>,decltype(compare)> queue(compare);std::map<std::tuple<int,int,int>,int> distances;
 auto heuristic=[&](glm::ivec3 p){return std::abs(goal.x-(p.x+.5f))+std::abs(goal.z-(p.z+.5f))+2*std::abs(goal.y-p.y);};
 auto start=glm::ivec3(glm::floor(a.position));queue.push({start,{0,0},0,0,heuristic(start)});distances[{start.x,start.y,start.z}]=0;float best=heuristic(start);glm::vec2 answer(0);unsigned expanded=0;int order=0;
 while(!queue.empty()&&expanded<256){auto n=queue.top();queue.pop();if(distances[{n.p.x,n.p.y,n.p.z}]!=n.distance)continue;++expanded;auto h=heuristic(n.p);if(h<best){best=h;answer=n.first;}if(h<1.1f){answer=n.first;break;}
  for(auto d:{glm::ivec3(1,0,0),glm::ivec3(0,0,1),glm::ivec3(-1,0,0),glm::ivec3(0,0,-1)}){
   auto q=n.p+d;if(std::abs(q.x-start.x)>24||std::abs(q.z-start.z)>24||!w.hasLoadedChunkAt(q.x,q.z))continue;
   for(int rise:{0,1,-1}){auto p=q+glm::ivec3(0,rise,0);auto key=std::make_tuple(p.x,p.y,p.z);int cost=n.distance+1+(rise?1:0);auto old=distances.find(key);if(old!=distances.end()&&old->second<=cost)continue;
    auto ground=w.getBlock(p.x,p.y-1,p.z);if(!isSolid(ground)||ground==Block::Cactus||isWater(ground)||isLava(ground))continue;
    glm::vec3 feet=glm::vec3(p)+glm::vec3(.5f,.01f,.5f);if(!villageClear(w,feet))continue;
    if(distances.size()>=1024)break;auto first=n.p==start?glm::vec2(feet.x-a.position.x,feet.z-a.position.z):n.first;distances[key]=cost;queue.push({p,first,cost,++order,cost+heuristic(p)});break;
   }
  }
 }
 villageNodesLastTick_+=expanded;return answer;
}
