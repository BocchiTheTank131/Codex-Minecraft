#include "Automation.h"
#include "World.h"
#include "Building.h"
#include "Farming.h"
#include "Player.h"
#include "Sound.h"
#include "SaveValidation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <istream>
#include <ostream>

namespace {
constexpr glm::ivec3 neighbors[]={{0,0,-1},{1,0,0},{0,0,1},{-1,0,0},{0,1,0},{0,-1,0}};
constexpr unsigned Budget=2048, CommitBudget=512, MaxNodes=100000;
int chunk(int n){return n>=0?n/16:(n+1)/16-1;}
Block at(const World& w,glm::ivec3 p){return w.getBlock(p.x,p.y,p.z);}
Block deviceType(Block b){return isDoor(b)?Block::WoodenDoor:blockType(b);}
bool directional(Block b){auto t=blockType(b);return t==Block::Repeater||t==Block::AndGate||t==Block::OrGate||t==Block::NotGate||t==Block::Observer;}
bool intersects(glm::vec3 a,glm::vec3 b,glm::vec3 c,glm::vec3 d){return a.x<d.x&&b.x>c.x&&a.y<d.y&&b.y>c.y&&a.z<d.z&&b.z>c.z;}
bool mature(const World& w,glm::ivec3 p){auto b=at(w,p);return (isCrop(b)&&cropStage(b)==3&&cropBase(b)<=Block::Beetroot0)||b==Block::Pumpkin||b==Block::Melon||b==Block::BerryBush3||(b==Block::SugarCane&&at(w,p-glm::ivec3(0,1,0))==Block::SugarCane);}
bool merge(const ItemStack& a,const ItemStack& b){return a.empty()||(a.item==b.item&&a.durability==b.durability&&a.identity==b.identity&&Inventory::maxDurability(a.item)==0);}
bool insert(ItemStack* slots,int count,ItemStack stack){
 for(int pass=0;pass<2;++pass)for(int i=0;i<count&&!stack.empty();++i){auto& dst=slots[i];if((pass==0)==dst.empty()||!merge(dst,stack))continue;int n=std::min(stack.count,Inventory::maxStack(stack.item)-(dst.empty()?0:dst.count));if(n<=0)continue;if(dst.empty()){dst=stack;dst.count=0;}dst.count+=n;stack.count-=n;}
 return stack.count==0;
}
template<class T>bool readValue(std::istream& in,T& v){return bool(in.read(reinterpret_cast<char*>(&v),sizeof(v)));}
template<class T>void writeValue(std::ostream& out,const T& v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));}
}
bool automationBlock(Block b){auto t=blockType(b);return t>=Block::PowerWire&&t<=Block::PistonHead;}
bool automationDevice(Block b){if(automationBlock(b))return blockType(b)!=Block::PistonHead;auto d=buildingDefinition(b);return (d&&(d->shape==BuildingShape::Button||d->shape==BuildingShape::PressurePlate||d->shape==BuildingShape::Door||d->shape==BuildingShape::Trapdoor||d->shape==BuildingShape::FenceGate))||isDoor(b);}
int automationSignal(Block b){auto d=buildingDefinition(b);if(d&&(d->shape==BuildingShape::Button||d->shape==BuildingShape::PressurePlate))return blockProperties(b)&8?15:0;if(blockType(b)==Block::Lever)return blockProperties(b)&8?15:0;auto t=blockType(b);return t==Block::PowerWire||directional(b)||t==Block::DaylightSensor||t==Block::CropSensor?(blockProperties(b)>>8)&15:0;}
glm::ivec3 automationFacing(Block b){auto p=blockProperties(b);if(blockType(b)==Block::Hopper&&(p&4))return {0,-1,0};return p&4?glm::ivec3(0,p&1?-1:1,0):buildingFacing(p);}
bool validAutomationState(Block b){auto t=blockType(b);unsigned p=blockProperties(b),mask=0;
 if(t==Block::PowerWire)mask=0xfff0;
 else if(t==Block::Lever)mask=11;
 else if(t==Block::Repeater)mask=0x0f33;
 else if(t==Block::Piston||t==Block::StickyPiston||t==Block::PistonHead)mask=15;
 else if(t==Block::Hopper)mask=15;
 else if(t==Block::AutoHarvester)mask=67;
 else if(t==Block::DaylightSensor||t==Block::PowerLamp)mask=0x0f00;
 else mask=0x0f03;
 if(p&~mask)return false;
 if((t==Block::Piston||t==Block::StickyPiston||t==Block::PistonHead||t==Block::Hopper)&&(p&4)&&(p&2))return false;
 if(t==Block::Hopper&&(p&4)&&(p&3))return false;
 if(t==Block::PowerWire)for(int d=0;d<4;++d)if((p&(4096<<d))&&!(p&(16<<d)))return false;
 return automationBlock(b);
}
const char* automationDescription(Block b){switch(blockType(b)){
 case Block::PowerWire:return "SIGNAL 0-15 / LOSES 1 PER WIRE";
 case Block::Lever:return "RMB: TOGGLE POWER";
 case Block::Repeater:return "REAR INPUT / RMB: DELAY 1-4 TICKS";
 case Block::AndGate:return "LEFT AND RIGHT INPUT / FRONT OUTPUT";
 case Block::OrGate:return "LEFT OR RIGHT INPUT / FRONT OUTPUT";
 case Block::NotGate:return "REAR INPUT / INVERTED FRONT OUTPUT";
 case Block::Hopper:return "PULL ABOVE / PUSH TO NOZZLE / POWER LOCKS";
 case Block::AutoHarvester:return "FRONT 3X3 / OUTPUT BELOW / RMB: REPLANT";
 case Block::CropSensor:return "MATURE CROPS IN FRONT 3X3";
 case Block::Observer:return "WATCHES REAR / FRONT PULSE";
 case Block::DaylightSensor:return "SKY LIGHT TIMES DAYLIGHT / 0-15";
 case Block::Piston:case Block::StickyPiston:return "PUSH UP TO 12 / BLOCK ENTITIES IMMOVABLE";
 default:return "POWERED BY ADJACENT SIGNAL";
}}
void Automation::reset(){*this=Automation{};}
unsigned Automation::remainingTicks(LogicPosition p,const Node& n)const{
 if(!n.due)return n.remaining;
 // At most 100k timers and 2048 due entries per tick => < 60 ticks of
 // scheduling backlog. Keep 128 range samples; fail frozen if ever exceeded.
 if(stats_.ticks-n.timerAt>=ranges_.size())return n.remaining;
 unsigned remaining=n.remaining;
 for(auto t=n.timerAt+1;t<=stats_.ticks&&remaining;++t){const auto& r=ranges_[t%ranges_.size()];if(r.tick!=t)continue;auto dx=std::int64_t(chunk(p.x))-r.x,dz=std::int64_t(chunk(p.z))-r.z;if(!r.valid||dx*dx+dz*dz<=std::int64_t(r.radius)*r.radius)--remaining;}
 return remaining;
}
void Automation::schedule(LogicPosition p,unsigned ticks){auto it=nodes_.find(p);if(it==nodes_.end())return;auto& n=it->second;if(n.due)scheduled_.erase({n.due,p});n.remaining=std::uint16_t(std::min(ticks,65535U));n.timerAt=stats_.ticks;n.due=stats_.ticks+std::max(1U,ticks);scheduled_[{n.due,p}]=true;}
void Automation::pause(LogicPosition p){
 active_.erase(p);dirty_.erase(p);wave_.erase(p);auto it=nodes_.find(p);if(it==nodes_.end())return;auto& n=it->second;
 if(n.due){n.remaining=std::uint16_t(remainingTicks(p,n));scheduled_.erase({n.due,p});n.due=0;}
}
bool Automation::canRegisterPlan(const std::vector<std::pair<glm::ivec3,Block>>& plan)const{
 std::int64_t count=std::int64_t(nodes_.size());
 for(std::size_t i=0;i<plan.size();++i){const auto& e=plan[i];for(std::size_t j=0;j<i;++j)if(plan[j].first==e.first)return false;
  count+=int(automationDevice(e.second))-int(nodes_.count(e.first)!=0);
 }return count>=0&&count<=MaxNodes;
}
void Automation::wake(glm::ivec3 p){if(active_.count(p))dirty_.insert(p);for(auto d:neighbors){auto q=p+d;if(active_.count(q))dirty_.insert(q);if(d.y==0)for(int y:{-1,1})if(active_.count(q+glm::ivec3(0,y,0)))dirty_.insert(q+glm::ivec3(0,y,0));}}
void Automation::changed(World& w,glm::ivec3 p,Block previous,Block next){
 auto it=nodes_.find(p);
 if(deviceType(previous)!=deviceType(next)&&it!=nodes_.end()){if(it->second.type==Block::Observer)--observerCount_;if(it->second.due)scheduled_.erase({it->second.due,p});nodes_.erase(it);auto group=chunks_.find({chunk(p.x),chunk(p.z)});if(group!=chunks_.end()){group->second.erase(p);if(group->second.empty())chunks_.erase(group);}active_.erase(p);dirty_.erase(p);}
 if(automationDevice(next)){
  it=nodes_.find(p);if(it==nodes_.end()&&nodes_.size()<MaxNodes){Node n;n.revision=++revision_;n.type=deviceType(next);n.observed=at(w,p-buildingFacing(blockProperties(next)));it=nodes_.emplace(p,n).first;if(n.type==Block::Observer)++observerCount_;chunks_[{chunk(p.x),chunk(p.z)}].insert(p);}
  if(it!=nodes_.end()&&!applying_)it->second.revision=++revision_;
  if(it!=nodes_.end()&&ready(w,p)){active_.insert(p);if(!it->second.due)schedule(p,std::max(1,int(it->second.remaining)));}
 }
 if(observerCount_&&previous!=next)for(auto d:neighbors){auto q=p+d;if(!w.hasLoadedChunkAt(q.x,q.z))continue;auto ob=nodes_.find(q);if(ob==nodes_.end()||ob->second.type!=Block::Observer)continue;auto state=at(w,q);if(q-buildingFacing(blockProperties(state))!=p)continue;ob->second.observed=next;ob->second.target=15;if(automationSignal(state)>0)schedule(q,2);if(active_.count(q))dirty_.insert(q);}
 wake(p);
}
void Automation::activateChunk(World& w,int x,int z){
 if(reconcilePending_&&reconcileStarted_)reconcileAgain_=true;reconcilePending_=true;
 auto c=w.findChunk(x,z);if(!c)return;
 for(auto i:c->automationCells){glm::ivec3 p(x*16+int(i%16),c->minimumY+int(i/256),z*16+int(i/16%16));auto b=at(w,p);if(automationDevice(b))changed(w,p,b,b);}
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto it=chunks_.find({x+dx,z+dz});if(it==chunks_.end())continue;for(auto p:it->second)if(ready(w,p)){active_.insert(p);dirty_.insert(p);auto& n=nodes_.at(p);if(!n.due)schedule(p,std::max(1,int(n.remaining)));}}
}
void Automation::suspendChunk(int x,int z){
 if(reconcilePending_&&reconcileStarted_)reconcileAgain_=true;reconcilePending_=true;
 auto pause=[&](LogicPosition p){this->pause(p);wake(glm::ivec3(p));};
 for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){auto group=chunks_.find({x+dx,z+dz});if(group==chunks_.end())continue;for(auto p:group->second){auto type=nodes_.at(p).type;const int radius=(type==Block::Piston||type==Block::StickyPiston)?13:(type==Block::CropSensor||type==Block::AutoHarvester)?3:1;bool touches=chunk(p.x-radius)<=x&&chunk(p.x+radius)>=x&&chunk(p.z-radius)<=z&&chunk(p.z+radius)>=z;if(touches)pause(p);}}
}

bool Automation::ready(const World& w,glm::ivec3 p)const{
 if(!w.hasLoadedChunkAt(p.x,p.z)||!w.simulationActiveAt(float(p.x),float(p.z)))return false;
 for(auto d:neighbors)if(!w.hasLoadedChunkAt(p.x+d.x,p.z+d.z))return false;
 auto b=at(w,p);auto t=blockType(b);
 if(t==Block::AutoHarvester||t==Block::CropSensor){auto f=buildingFacing(blockProperties(b)),side=buildingFacing(blockProperties(b)+1);for(int z=1;z<=3;++z)for(int x:{-1,1}){auto q=p+f*z+side*x;if(!w.hasLoadedChunkAt(q.x,q.z))return false;}}
 if(t==Block::Piston||t==Block::StickyPiston){auto f=automationFacing(b);for(int n=1;n<=13;++n){auto q=p+f*n;if(!w.hasLoadedChunkAt(q.x,q.z))return false;}}
 return true;
}
int Automation::input(const World& w,glm::ivec3 p,int dir)const{
 int power=0;for(int i=0;i<6;++i){if(dir>=0&&dir!=i)continue;auto q=p+neighbors[i];auto b=at(w,q);int value=automationSignal(b);if(directional(b)&&q+automationFacing(b)!=p)value=0;power=std::max(power,value);
  if(i<4&&dir<0){if(isSolid(b)&&at(w,p+glm::ivec3(0,1,0))==Block::Air){auto above=at(w,q+glm::ivec3(0,1,0));if(blockType(above)==Block::PowerWire)power=std::max(power,automationSignal(above));}else if(!isSolid(b)){auto below=at(w,q-glm::ivec3(0,1,0));if(blockType(below)==Block::PowerWire)power=std::max(power,automationSignal(below));}}
 }return power;
}
bool Automation::interact(World& w,glm::ivec3 p){auto b=at(w,p);auto t=blockType(b);auto props=blockProperties(b);if(t==Block::Lever)props^=8;else if(t==Block::Repeater)props=(props&~48)|(((props+16)&48));else if(t==Block::AutoHarvester)props^=64;else{auto d=buildingDefinition(b);if(!d||d->shape!=BuildingShape::Button)return false;props|=8;}w.setBlock(p.x,p.y,p.z,blockState(b,props));if(auto d=buildingDefinition(b);d&&d->shape==BuildingShape::Button)schedule(p,5);return true;}
void Automation::update(float dt,World& w,SurvivalWorld& mobs,const Player* player,float light,SoundSystem* sounds){
 if(!std::isfinite(dt)||dt<=0)return;const auto started=std::chrono::steady_clock::now();
 // SimulationClock already bounds catch-up. Preserve fractional elapsed time.
 accumulator_+=dt;unsigned steps=0;while(accumulator_>=.1&&steps++<4){accumulator_-=.1;tick(w,mobs,player,std::clamp(std::isfinite(light)?light:0.f,0.f,1.f),sounds);}
 stats_.devices=nodes_.size();stats_.dirty=dirty_.size()+wave_.size()+changes_.size();stats_.milliseconds=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-started).count();
}
void Automation::tick(World& w,SurvivalWorld& mobs,const Player* player,float light,SoundSystem* sounds){
 const auto tickStart=std::chrono::steady_clock::now();
 stats_.processed=stats_.changes=0;stats_.reconciled=stats_.dueProcessed=0;
 unsigned soundBudget=2;auto audible=[&](glm::ivec3 p,float radius){if(!sounds||!player||soundBudget==0||glm::length(glm::vec3(p)-player->position())>=radius)return false;--soundBudget;return true;};
 RangeSample range{stats_.ticks+1,w.streamCenter_.x,w.streamCenter_.y,w.simulationDistance_,w.streamCenterValid_};
 const bool moved=range.x!=lastRange_.x||range.z!=lastRange_.z||range.radius!=lastRange_.radius||range.valid!=lastRange_.valid;
 if(moved){if(reconcilePending_&&reconcileStarted_)reconcileAgain_=true;reconcilePending_=true;lastRange_=range;}
 // Eligibility discovery is incremental and fair, including remote saved
 // registrations. Timer clocks below stop at the actual range transition,
 // independently of when this bounded bookkeeping reaches a given device.
 while(reconcilePending_&&stats_.reconciled<512){auto it=reconcileStarted_?nodes_.upper_bound(reconcileCursor_):nodes_.begin();
  if(it==nodes_.end()){reconcilePending_=reconcileAgain_;reconcileAgain_=false;reconcileStarted_=false;break;}
  auto p=it->first;reconcileCursor_=p;reconcileStarted_=true;++stats_.reconciled;
  if(!ready(w,p))pause(p);else if(!active_.count(p)){active_.insert(p);dirty_.insert(p);if(!it->second.due)schedule(p,std::max(1,int(it->second.remaining)));}
 }
 ++stats_.ticks;
 ranges_[stats_.ticks%ranges_.size()]=range;
 unsigned dueWork=0;
 while(!scheduled_.empty()&&scheduled_.begin()->first.first<=stats_.ticks&&dueWork<Budget){++dueWork;auto key=scheduled_.begin()->first;scheduled_.erase(scheduled_.begin());auto it=nodes_.find(key.second);if(it==nodes_.end()||it->second.due!=key.first)continue;auto& n=it->second;const auto remaining=remainingTicks(key.second,n);n.due=0;n.remaining=std::uint16_t(remaining);auto p=glm::ivec3(key.second);
  if(!ready(w,p)){active_.erase(p);dirty_.erase(p);wave_.erase(p);continue;}
  if(remaining){schedule(p,remaining);continue;}dirty_.insert(p);
 }
 stats_.dueProcessed=dueWork;
 if(wave_.empty()&&changes_.empty())wave_.swap(dirty_);
 const auto prepared=std::chrono::steady_clock::now();stats_.preparationMilliseconds=std::chrono::duration<float,std::milli>(prepared-tickStart).count();
 // All signal reads in a wave precede its writes, including when budget-sliced.
 unsigned processed=0;
 while(!wave_.empty()&&processed++<Budget){auto pos=*wave_.begin();auto it=nodes_.find(pos);if(it==nodes_.end()){wave_.erase(wave_.begin());continue;}auto p=glm::ivec3(pos);auto& n=it->second;auto b=at(w,p);if(!ready(w,p)||deviceType(b)!=n.type){wave_.erase(wave_.begin());continue;}auto t=blockType(b);
  const unsigned cost=(t==Block::Piston||t==Block::StickyPiston)?15:t==Block::AutoHarvester?18:t==Block::Hopper?4:1;
  if(processed+cost-1>Budget){--processed;break;}processed+=cost-1;wave_.erase(wave_.begin());
  auto d=buildingDefinition(b);unsigned props=blockProperties(b);int signal=input(w,p),out=automationSignal(b),next=out;
  if(t==Block::PowerWire){props=blockProperties(buildingConnectedState(w,p,b));next=0;for(int k=0;k<6;++k){auto q=p+neighbors[k];auto v=at(w,q);int s=automationSignal(v);if(directional(v)&&q+automationFacing(v)!=p)s=0;next=std::max(next,s-(blockType(v)==Block::PowerWire?1:0));if(k<4){auto wireAt=[&](int y){auto wire=at(w,q+glm::ivec3(0,y,0));if(blockType(wire)==Block::PowerWire)next=std::max(next,automationSignal(wire)-1);};if(isSolid(v)&&at(w,p+glm::ivec3(0,1,0))==Block::Air)wireAt(1);else if(!isSolid(v))wireAt(-1);}}next=std::clamp(next,0,15);}
  else if(t==Block::Repeater){int target=input(w,p,((props&3)+2)&3)?15:0;if(n.target!=target){n.target=std::uint8_t(target);schedule(pos,1+((props>>4)&3));}else if(!n.due)next=n.target;}
  else if(t==Block::AndGate||t==Block::OrGate){bool a=input(w,p,((props&3)+1)&3)>0,c=input(w,p,((props&3)+3)&3)>0;next=(t==Block::AndGate?(a&&c):(a||c))?15:0;}
  else if(t==Block::NotGate)next=input(w,p,((props&3)+2)&3)?0:15;
  else if(t==Block::DaylightSensor){next=int(std::lround(w.sunlightAt(p.x,p.y+1,p.z)*light));if(!n.due)schedule(pos,10);}
  else if(t==Block::CropSensor){next=0;bool complete=true;auto f=buildingFacing(props),side=buildingFacing(props+1);for(int z=1;z<=3;++z)for(int x=-1;x<=1;++x){auto q=p+f*z+side*x;if(!w.hasLoadedChunkAt(q.x,q.z)){complete=false;continue;}if(mature(w,q)||mature(w,q-glm::ivec3(0,1,0)))next=15;}if(!complete)next=out;if(!n.due)schedule(pos,5);}
  else if(t==Block::Observer){auto sample=at(w,p-buildingFacing(props));if(sample!=n.observed){n.observed=sample;n.target=15;if(out>0)schedule(pos,2);}else if(out>0&&!n.due)n.target=0;next=n.target;}
  else if(d&&d->shape==BuildingShape::Button){if(!n.due)props&=~8;}
  else if(d&&d->shape==BuildingShape::PressurePlate){auto a=glm::vec3(p)+glm::vec3(.0625f,0,.0625f),c=glm::vec3(p)+glm::vec3(.9375f,.25f,.9375f);bool pressed=(player&&!player->isSpectator()&&!player->isDead()&&intersects(a,c,player->aabbMinimum(),player->aabbMaximum()))||mobs.anyMobIntersects(a,c);props=pressed?props|8:props&~8;if(!n.due)schedule(pos,1);}
  else if(t==Block::PowerLamp)props=(props&~0x0f00)|((signal?15:0)<<8);
  else if(t==Block::Hopper){props=signal?props|8:props&~8;if(!signal&&!n.due){if(transfer(w,p,b)){++stats_.transfers;if(audible(p,12))sounds->playItemPickup();}schedule(pos,4);}}
  else if(t==Block::Piston||t==Block::StickyPiston){if((signal>0)!=bool(props&8)||((props&8)&&blockType(at(w,p+automationFacing(b)))!=Block::PistonHead)){if(piston(w,mobs,player,p,b,signal>0)){++stats_.moves;if(audible(p,24))sounds->playBlockPlace(Block::IronBlock,glm::vec3(p));}else if(!n.due)schedule(pos,5);}n.input=std::uint8_t(signal);continue;}
  else if(t==Block::AutoHarvester){if(signal&&!n.due){harvest(w,p,b);schedule(pos,4);}else if(!n.due)schedule(pos,4);}
  else if((d&&(d->shape==BuildingShape::Door||d->shape==BuildingShape::Trapdoor||d->shape==BuildingShape::FenceGate))||isDoor(b)){
   const bool pair=(d&&d->shape==BuildingShape::Door)||isDoor(b);
   const bool upper=isDoor(b)?isDoorUpper(b):bool(props&16);
   if(pair&&upper){dirty_.insert(p-glm::ivec3(0,1,0));continue;}
   if(pair){auto q=p+glm::ivec3(0,1,0);auto mate=at(w,q);if(!ready(w,q)||!nodes_.count(q)||(isDoor(b)?!isDoor(mate)||!isDoorUpper(mate)||doorFacing(mate)!=doorFacing(b)||doorHingeRight(mate)!=doorHingeRight(b):blockType(mate)!=t||!(blockProperties(mate)&16)))continue;signal=std::max(input(w,p),input(w,q));}
   if(bool(signal)!=bool(n.input)){
    const bool open=signal>0;auto desired=isDoor(b)?doorBlock(doorFacing(b),false,doorHingeRight(b),open):blockState(b,open?props|8:props&~8);
    // Even an already-closed manual override must acknowledge the falling
    // electrical edge. Commit the input together with both physical halves.
    changes_.push_back({pos,b,desired,n.revision,signal});
    if(desired!=b&&audible(p,24))sounds->playDoor(open,glm::vec3(p));
   }
   continue;
  }
  const bool door=(d&&(d->shape==BuildingShape::Door||d->shape==BuildingShape::Trapdoor||d->shape==BuildingShape::FenceGate))||isDoor(b);
  if(!door)n.input=std::uint8_t(signal);
  if(t==Block::PowerWire||directional(b)||t==Block::DaylightSensor||t==Block::CropSensor)props=(props&~0x0f00)|(next<<8);
  auto after=blockState(b,std::uint16_t(props));if(after!=b)changes_.push_back({pos,b,after,n.revision});
 }
 stats_.processed=std::min(processed,Budget);
 const auto evaluated=std::chrono::steady_clock::now();stats_.evaluationMilliseconds=std::chrono::duration<float,std::milli>(evaluated-prepared).count();
 if(wave_.empty()){
  unsigned writes=0,attempts=0;while(!changes_.empty()&&attempts<CommitBudget&&writes+2<=CommitBudget){++attempts;auto c=changes_.back();changes_.pop_back();auto p=glm::ivec3(c.position);auto current=nodes_.find(c.position);
   if(current==nodes_.end()||current->second.revision!=c.revision||at(w,p)!=c.before||!ready(w,p))continue;
   if(c.electricalInput<0){applying_=true;const bool committed=w.setBlockInternal(p.x,p.y,p.z,c.after,true,false);applying_=false;if(committed){++writes;++stats_.changes;if(blockType(c.after)==Block::Observer&&automationSignal(c.after)>0&&automationSignal(c.before)==0)schedule(c.position,2);}continue;}
   auto d=buildingDefinition(c.before);const bool pair=isDoor(c.before)||(d&&d->shape==BuildingShape::Door);
   std::vector<std::pair<glm::ivec3,Block>> before{{p,c.before}},after{{p,c.after}};
   if(c.electricalInput>=0&&pair){auto q=p+glm::ivec3(0,1,0);auto mate=at(w,q);if(!ready(w,q)||!nodes_.count(q)||(isDoor(c.before)?!isDoor(mate)||!isDoorUpper(mate)||doorFacing(mate)!=doorFacing(c.before)||doorHingeRight(mate)!=doorHingeRight(c.before):blockType(mate)!=blockType(c.before)||!(blockProperties(mate)&16))){dirty_.insert(p);continue;}
    auto desired=isDoor(mate)?doorBlock(doorFacing(mate),true,doorHingeRight(mate),c.electricalInput>0):blockState(mate,c.electricalInput?blockProperties(mate)|8:blockProperties(mate)&~8);before.push_back({q,mate});after.push_back({q,desired});}
   applying_=true;const bool committed=w.automationBatch(before,after);applying_=false;
   if(committed){if(c.electricalInput>=0)for(auto e:after)nodes_.at(e.first).input=std::uint8_t(c.electricalInput);if(blockType(c.after)==Block::Observer&&automationSignal(c.after)>0&&automationSignal(c.before)==0)schedule(c.position,2);writes+=unsigned(after.size());stats_.changes+=after.size();}
  }
  if(!changes_.empty())++stats_.overloadTicks;
 }else ++stats_.overloadTicks;
 const auto ended=std::chrono::steady_clock::now();stats_.commitMilliseconds=std::chrono::duration<float,std::milli>(ended-evaluated).count();stats_.tickMilliseconds=std::chrono::duration<float,std::milli>(ended-tickStart).count();
}

bool Automation::transfer(World& w,glm::ivec3 p,Block b){
 auto* hopper=w.chestAt(p,true);if(!hopper)return false;
 auto destination=p+automationFacing(b),source=p+glm::ivec3(0,1,0);
 auto move=[&](glm::ivec3 from,glm::ivec3 to){
  if(!ready(w,from)||!ready(w,to))return false;
  auto fb=at(w,from),tb=at(w,to);std::array<ItemStack*,27> inputs{},outputs{};int inputCount=0,outputCount=0;
  if(auto* c=w.chestAt(from);c&&(fb==Block::Chest||buildingIsContainer(fb)))for(auto& stack:c->slots)inputs[inputCount++]=&stack;
  if(auto* f=w.furnaceAt(from);f)inputs[inputCount++]=&f->output;
  if(auto* c=w.chestAt(to);c&&(tb==Block::Chest||buildingIsContainer(tb)))for(auto& stack:c->slots)outputs[outputCount++]=&stack;
  auto* furnace=w.furnaceAt(to);
  for(int i=0;i<inputCount;++i){auto* src=inputs[i];if(src->empty())continue;auto unit=*src;unit.count=1;
   ItemStack* furnaceTarget=nullptr;if(furnace){if(from.y>to.y){if(itemDefinition(unit.item).smeltedItem!=Item::None)furnaceTarget=&furnace->input;}else if(furnaceFuelSeconds(unit.item)>0)furnaceTarget=&furnace->fuel;}
   const int count=outputCount+(furnaceTarget?1:0);
   for(int j=0;j<count;++j){auto* dst=j<outputCount?outputs[j]:furnaceTarget;if(merge(*dst,unit)&&(dst->empty()||dst->count<Inventory::maxStack(unit.item))){if(dst->empty())*dst=unit;else ++dst->count;if(--src->count==0)*src={};return true;}}
  }return false;
 };
 // One item operation per cooldown, push first. No pointers survive world edits.
 if(move(p,destination))return true;return move(source,p);
}

bool Automation::piston(World& w,SurvivalWorld& mobs,const Player* player,glm::ivec3 p,Block b,bool extend){
 const auto f=automationFacing(b);const auto head=p+f;auto movable=[&](Block v){if(v==Block::Air)return true;if(automationDevice(v)||blockType(v)==Block::PistonHead||v==Block::Bedrock||v==Block::Chest||v==Block::Furnace||isDoor(v))return false;if(isBuilding(v)){auto d=buildingDefinition(v);return d&&(d->shape==BuildingShape::Cube||d->shape==BuildingShape::Stairs||d->shape==BuildingShape::Slab||d->shape==BuildingShape::StrippedLog||d->shape==BuildingShape::Path);}return isSolid(v)&&!isWater(v)&&!isLava(v)&&(blockGeometry(v).shape==BlockShape::Cube||isSlab(v));};
 auto eligible=[&](glm::ivec3 q){if(!movable(at(w,q)))return false;for(auto offset:neighbors){auto support=blockSupportPosition(q+offset,at(w,q+offset));if(support&&*support==q)return false;}return true;};
  auto clearOfActors=[&](glm::ivec3 q,Block v){auto boxes=w.blockBoxes(q,true);if(isBuilding(v))boxes=buildingBoxes(v,true);else {boxes={};auto g=blockGeometry(v);boxes.add(g.minX,g.minY,g.minZ,g.maxX,g.maxY,g.maxZ);}for(int i=0;i<boxes.count;++i){auto a=glm::vec3(q)+boxes.boxes[i].minimum,c=glm::vec3(q)+boxes.boxes[i].maximum;if((player&&!player->isSpectator()&&intersects(a,c,player->aabbMinimum(),player->aabbMaximum()))||mobs.anyMobIntersects(a,c)||(additionalCollision&&additionalCollision(a,c)))return false;}return true;};
 std::vector<std::pair<glm::ivec3,Block>> before,after;auto add=[&](glm::ivec3 q,Block v){before.push_back({q,at(w,q)});after.push_back({q,v});};
 if(extend){if(blockProperties(b)&8){if(blockType(at(w,head))==Block::PistonHead)return false;add(p,blockState(b,blockProperties(b)&~8));return w.automationBatch(before,after);}int count=0;auto q=head;
  while(at(w,q)!=Block::Air){if(!ready(w,q)||count>=12||!eligible(q))return false;
   ++count;q+=f;}
  if(!ready(w,q))return false;
  for(int i=count;i>=1;--i){auto dest=head+f*i,src=dest-f;auto value=at(w,src);if(!clearOfActors(dest,value))return false;add(dest,value);}
  auto headState=blockState(Block::PistonHead,blockProperties(b)&7);if(!clearOfActors(head,headState))return false;add(head,headState);add(p,blockState(b,blockProperties(b)|8));
 }else{if(!(blockProperties(b)&8))return false;if(!ready(w,head)||!clearOfActors(p,blockState(b,blockProperties(b)&~8)))return false;auto hb=at(w,head);if(blockType(hb)==Block::PistonHead&&(blockProperties(hb)&7)==(blockProperties(b)&7))add(head,Block::Air);
  if(blockType(b)==Block::StickyPiston&&blockType(hb)==Block::PistonHead&&(blockProperties(hb)&7)==(blockProperties(b)&7)){auto source=head+f;auto value=at(w,source);if(ready(w,source)&&value!=Block::Air&&eligible(source)&&clearOfActors(head,value)){after.back().second=value;add(source,Block::Air);}}
  add(p,blockState(b,blockProperties(b)&~8));
 }return w.automationBatch(before,after);
}

bool Automation::harvest(World& w,glm::ivec3 p,Block b){
 auto output=p-glm::ivec3(0,1,0);if(!ready(w,output))return false;auto container=at(w,output);if(container!=Block::Chest&&!buildingIsContainer(container))return false;auto* chest=w.chestAt(output,true);if(!chest)return false;
 auto f=buildingFacing(blockProperties(b)),side=buildingFacing(blockProperties(b)+1);
 for(int z=1;z<=3;++z)for(int x=-1;x<=1;++x){auto q=p+f*z+side*x;if(!ready(w,q)||!mature(w,q))continue;auto crop=at(w,q);auto slots=chest->slots;auto drops=FarmingSystem::harvest(crop,q,w.seed());Block replacement=Block::Air;
  if(crop==Block::SugarCane){
   // Harvest the uppermost segment first. Never detach an uncollected segment.
   while(q.y+1<w.maximumY()&&at(w,q+glm::ivec3(0,1,0))==Block::SugarCane){if(!ready(w,q+glm::ivec3(0,1,0)))return false;++q.y;}
   drops=FarmingSystem::harvest(crop,q,w.seed());
  }
  if(crop==Block::BerryBush3)replacement=Block::BerryBush1;
  else if(isCrop(crop)&&!(blockProperties(b)&64)){auto base=cropBase(crop);Item seed=base==Block::Crop0?Item::Seeds:base==Block::Carrot0?Item::Carrot:base==Block::Potato0?Item::Potato:Item::BeetrootSeeds;bool paid=false;for(auto& drop:drops)if(drop.item==seed&&drop.count){--drop.count;paid=true;break;}if(!paid)for(auto& s:slots)if(s.item==seed&&s.count){if(--s.count==0)s={};paid=true;break;}if(paid&&w.canPlacePlant(q,base))replacement=base;else return false;}
  for(auto stack:drops)if(!stack.empty()&&!insert(slots.data(),int(slots.size()),stack))return false;
  if(!w.automationBatch({{q,crop}},{{q,replacement}}))return false;
  chest->slots=slots;return true;
 }return false;
}

bool World::automationBatch(const std::vector<std::pair<glm::ivec3,Block>>& before,const std::vector<std::pair<glm::ivec3,Block>>& after){
 if(before.size()!=after.size()||before.empty()||before.size()>15)return false;
 if(!automation.canRegisterPlan(after))return false;
 for(std::size_t i=0;i<before.size();++i){auto p=before[i].first;if(p!=after[i].first||p.y<minimumY()||p.y>=maximumY()||!hasLoadedChunkAt(p.x,p.z)||getBlock(p.x,p.y,p.z)!=before[i].second||!validBlockState(after[i].second))return false;if(blockType(before[i].second)!=blockType(after[i].second)&&!blockEntityContents(p).empty())return false;for(std::size_t j=0;j<i;++j)if(before[j].first==p)return false;}
 // No input, save capture or worker can observe the middle of this main-thread operation.
 automationMoving_=true;
 // Release registrations before additions, so replacement batches also work
 // when the registry is completely full. Validation above has reserved net use.
 for(auto edit:after)if(automationDevice(getBlock(edit.first.x,edit.first.y,edit.first.z))&&!automationDevice(edit.second))setBlockInternal(edit.first.x,edit.first.y,edit.first.z,edit.second,true,false);
 for(auto edit:after)if(getBlock(edit.first.x,edit.first.y,edit.first.z)!=edit.second)setBlockInternal(edit.first.x,edit.first.y,edit.first.z,edit.second,true,false);
 automationMoving_=false;
 for(auto edit:after)revalidateSupports(edit.first);
 for(auto edit:after){auto p=edit.first;for(int y=-1;y<=1;++y)for(auto d:neighbors){auto q=p+d+glm::ivec3(0,y,0);auto value=getBlock(q.x,q.y,q.z);if(isBuilding(value)){auto connected=buildingConnectedState(*this,q,value);if(value!=connected)setBlockInternal(q.x,q.y,q.z,connected,true,false);}}}
 return true;
}

bool Automation::valid()const{if(nodes_.size()>MaxNodes)return false;for(auto e:nodes_){auto p=e.first;auto n=e.second;if(!SaveValidation::position(glm::vec3(p.x,p.y,p.z))||!automationDevice(n.type)||n.type!=blockType(n.type)||n.remaining>10||(n.due>stats_.ticks+10)||n.target>15||n.input>15||!validBlockState(n.observed))return false;}return true;}
bool Automation::write(std::ostream& out)const{if(!valid())return false;out.write("AUT1",4);writeValue(out,std::uint32_t(nodes_.size()));for(auto e:nodes_){writeValue(out,e.first.x);writeValue(out,e.first.y);writeValue(out,e.first.z);writeValue(out,std::uint32_t(e.second.type));auto remaining=std::uint16_t(remainingTicks(e.first,e.second));writeValue(out,remaining);writeValue(out,e.second.target);writeValue(out,e.second.input);writeValue(out,std::uint32_t(e.second.observed));}return bool(out);}
bool Automation::read(std::istream& in,int low,int high){Automation candidate;char tag[4]{};std::uint32_t count=0;in.read(tag,4);if(!in||std::memcmp(tag,"AUT1",4)||!readValue(in,count)||count>MaxNodes)return false;for(unsigned i=0;i<count;++i){LogicPosition p;Node n;std::uint32_t t=0,observed=0;if(!readValue(in,p.x)||!readValue(in,p.y)||!readValue(in,p.z)||!readValue(in,t)||!readValue(in,n.remaining)||!readValue(in,n.target)||!readValue(in,n.input)||!readValue(in,observed)||p.y<low||p.y>=high)return false;n.type=Block(t);n.observed=Block(observed);if(!candidate.nodes_.emplace(p,n).second)return false;if(n.type==Block::Observer)++candidate.observerCount_;candidate.chunks_[{chunk(p.x),chunk(p.z)}].insert(p);}if(!candidate.valid())return false;*this=std::move(candidate);return true;}

bool Automation::placementsValid(const std::function<Block(glm::ivec3)>& savedBlock)const{
 for(const auto& entry:nodes_){auto block=savedBlock(glm::ivec3(entry.first));if(deviceType(block)!=entry.second.type&&!(block==Block::WoodenDoor&&isDoor(entry.second.type)))return false;}return true;
}
