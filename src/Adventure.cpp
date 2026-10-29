#include "Adventure.h"
#include "World.h"
#include "Player.h"
#include "SaveValidation.h"
#include "TerrainDensity.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace {
template<class T>bool get(std::istream& in,T& v){return bool(in.read(reinterpret_cast<char*>(&v),sizeof(v)));}
template<class T>void put(std::ostream& out,const T& v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));}
bool position(glm::ivec3 p){return p.x>-30000000&&p.x<30000000&&p.z>-30000000&&p.z<30000000&&p.y>=-64&&p.y<320;}
unsigned bits(std::uint64_t n){unsigned count=0;while(n){n&=n-1;++count;}return count;}
bool inside(glm::vec3 p,glm::ivec3 c){return std::abs(p.x-c.x-.5f)<6.5f&&std::abs(p.z-c.z-.5f)<6.5f&&p.y>=c.y&&p.y<c.y+5;}
bool ready(const World& w,const RelicChamber& c){for(int z:{-7,7})for(int x:{-7,7})if(!w.hasLoadedChunkAt(c.center.x+x,c.center.z+z))return false;return true;}
bool validProgress(std::uint64_t id,const RelicProgress& s){
    if(!position(s.center)||unsigned(s.kind)>2||s.sequence>3||s.trialTicks>100||
       id!=Adventure::regionId(adventureFloorDiv(s.center.x,256),adventureFloorDiv(s.center.z,256)))return false;
    if(s.flags!=0&&s.flags!=1&&s.flags!=3&&s.flags!=7&&s.flags!=15)return false;
    if((s.flags&1)&&s.kind!=RelicPuzzle::Routing&&s.sequence!=3)return false;
    if(s.kind==RelicPuzzle::Routing&&s.sequence!=0)return false;
    if(s.kind==RelicPuzzle::Sequence&&s.trialTicks!=0)return false;
    if(s.trialTicks&&!(s.flags&2))return false;
    if(s.kind!=RelicPuzzle::Sequence&&(s.flags&4)&&s.trialTicks!=100)return false;
    return true;
}
void open(World& w,const RelicChamber& c){const auto p=c.gate();const auto b=w.getBlock(p.x,p.y,p.z);if(blockType(b)==Block::OakFenceGate&&!(blockProperties(b)&8))w.setBlock(p.x,p.y,p.z,blockState(Block::OakFenceGate,blockProperties(b)|8));}
}
bool isCollectibleRelic(Item i){return i==Item::SurveyorToken||i==Item::CavernShard||i==Item::AncientSeal;}
bool isRelicTool(Item i){return i==Item::SurveyorCompass||i==Item::EchoLantern||i==Item::AncientExplorerTool;}
bool validRelicEncounterTag(std::uint64_t tag){const auto id=tag^0xa719000000000000ULL;return tag&&std::abs(std::int64_t(std::int32_t(id>>32)))<=117188&&std::abs(std::int64_t(std::int32_t(id)))<=117188;}
std::uint64_t Adventure::regionId(int x,int z){return (std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(z);}
void Adventure::notify(std::string text){if(notice_.empty())notice_=std::move(text);}
std::string Adventure::takeNotice(){std::string result;result.swap(notice_);return result;}
bool Adventure::discover(std::uint8_t kind,glm::ivec3 p){
    if(kind>8||!position(p)||discoveries_.size()>=MaxDiscoveries)return false;
    for(const auto& d:discoveries_)if(d.kind==kind&&d.position==p)return false;
    discoveries_.push_back({kind,p});notify(kind==7?"Journal: cave passage explored":"Journal: new exploration site discovered");return true;
}
void Adventure::biome(Biome b){const auto i=unsigned(b);if(i>=35)return;const auto bit=std::uint64_t(1)<<i;if(!(biomes&bit)){biomes|=bit;notify(std::string("Discovered biome: ")+biomeDefinition(b).name);}}
void Adventure::observeInventory(const Inventory& inv){
    crafted|=inv.craftedRelicTools&63;
    auto observe=[&](Item i){for(unsigned n=0;n<3;++n){if(i==Item(unsigned(Item::SurveyorToken)+n))relics|=1U<<n;}};
    for(int i=0;i<Inventory::TotalSlots;++i)observe(inv.slot(i).item);
    for(int i=0;i<Inventory::EquipmentSlots;++i)observe(inv.equipment(i).item);
    observe(inv.cursorStack().item);for(int i=0;i<Inventory::CraftSlots;++i)observe(inv.craftSlot(i).item);
}
RelicProgress* Adventure::progress(const RelicChamber& c){
    const auto id=regionId(c.regionX,c.regionZ);auto it=puzzles_.find(id);
    if(it!=puzzles_.end())return it->second.center==c.center&&it->second.kind==c.kind?&it->second:nullptr;
    if(puzzles_.size()>=MaxPuzzles)return nullptr;
    return &puzzles_.emplace(id,RelicProgress{c.center,c.kind}).first->second;
}
bool Adventure::locked(const RelicChamber& c)const{auto it=puzzles_.find(regionId(c.regionX,c.regionZ));return it==puzzles_.end()||!(it->second.flags&4);}
bool Adventure::useControl(World& w,const RelicChamber& c,glm::ivec3 p,std::string& message){
    if(p==c.gate()){
        if(blockType(w.getBlock(p.x,p.y,p.z))!=Block::OakFenceGate)return false;
        message=locked(c)?"Vault sealed: solve the chamber, then complete its trial":"Vault unlocked";return locked(c);
    }
    int n=-1;for(int i=0;i<3;++i)if(p==c.control(i))n=i;
    if(n<0||c.kind!=RelicPuzzle::Sequence||blockType(w.getBlock(p.x,p.y,p.z))!=Block::Lever)return false;
    auto state=progress(c);if(!state){message="Journal chamber limit reached; no progress consumed";return true;}
    if(state->flags&1){message="Three Bells completed; enter the room to face its finite ambush";return true;}
    if(blockType(w.getBlock(p.x,p.y,p.z))!=Block::Lever){message="Mechanism missing: restore a lever here";return true;}
    if(n==state->sequence){w.setBlock(p.x,p.y,p.z,blockState(Block::Lever,8));++state->sequence;message="Bell "+std::to_string(n+1)+" resonates";}
    else {for(int i=0;i<3;++i){auto q=c.control(i);if(blockType(w.getBlock(q.x,q.y,q.z))==Block::Lever)w.setBlock(q.x,q.y,q.z,Block::Lever);}state->sequence=0;message="Bells reset. Follow the one, two, three gold inlays, left to right";}
    if(state->sequence==3){state->flags|=1;message="Sequence solved. A finite ambush guards the vault";}
    return true;
}
bool Adventure::useTreasure(World& w,const RelicChamber& c,Inventory& inv,Player& player,bool creative,std::string& message){
    const auto chest=c.chest();if(blockType(w.getBlock(chest.x,chest.y,chest.z))!=Block::Chest){message="Original vault chest missing; no reward consumed";return true;}
    if(creative)return false; // sandbox access does not claim Survival rewards
    auto state=progress(c);if(!state){message="Chamber record capacity reached";return true;}
    if(locked(c)){message="Sealed treasure: complete the chamber and its challenge first";return true;}
    if(!(state->flags&8)){
        // Seal is a reusable key; it is explicitly never consumed. Contents remain
        // in their one original 27-slot chest, including if it was emptied/broken.
        bool hasSeal=inv.count(Item::AncientSeal)>0||inv.equipment(4).item==Item::AncientSeal||inv.cursorStack().item==Item::AncientSeal;
        for(int i=0;i<Inventory::CraftSlots;++i)hasSeal=hasSeal||inv.craftSlot(i).item==Item::AncientSeal;
        if(!creative&&!hasSeal){message="An Ancient Seal unlocks this vault without being consumed";return true;}
        state->flags|=8;if(!creative)player.addExperience(8);notify("Vault opened: Ancient Seal retained / journal updated");
    }
    return false;
}
void Adventure::update(float dt,World& w,SurvivalWorld& mobs,Player& p,Inventory& inv){
    if(!std::isfinite(dt)||dt<=0)return;accumulator_+=dt;
    unsigned steps=0;while(accumulator_>=.1f&&steps++<4){accumulator_-=.1f;
        if(compassCooldown_)--compassCooldown_;if(lanternCooldown_)--lanternCooldown_;
        if(p.isDead()||p.isSpectator())continue;
        const auto pos=p.position();if(!w.hasLoadedChunkAt(int(std::floor(pos.x)),int(std::floor(pos.z))))continue;
        // 10 Hz observes ownership; geographical/proximity checks are 2 Hz.
        observeInventory(inv);
        static_assert(unsigned(Biome::Count)==35,"Update bounded biome mask for new biomes");
        if(++discoveryTick_%5==0){
            biome(w.biomeClimateAt(int(std::floor(pos.x)),int(std::floor(pos.z)),pos.y).biome);
            for(const auto& d:w.nearbyAdventureSites(pos,12))discover(d.kind,d.position);
            const auto f=glm::ivec3(glm::floor(pos));
            if(f.y<w.generationDebugAt(f.x,f.z,f.y).terrainHeight-8&&w.getBlock(f.x,f.y,f.z)==Block::Air&&isSolid(w.getBlock(f.x,f.y-1,f.z)))
                discover(7,{adventureFloorDiv(f.x,64)*64,adventureFloorDiv(f.y,16)*16,adventureFloorDiv(f.z,64)*64});
        }
        const unsigned thresholds[]={1,5,15,35};for(unsigned i=0;i<4;++i)if(bits(biomes)>=thresholds[i]&&!(achievements&(1U<<i))){achievements|=1U<<i;if(!p.isCreative())p.addExperience(2+i*2);notify("Achievement: "+std::to_string(thresholds[i])+" biomes explored");}
        if(relics==7&&!(achievements&16)){achievements|=16;if(!p.isCreative())p.addExperience(6);notify("Achievement: Relic Collector");}
        if(w.generationVersion()<24)continue;
        const int rx=adventureFloorDiv(int(std::floor(pos.x)),256),rz=adventureFloorDiv(int(std::floor(pos.z)),256);
        const auto plan=w.cachedUndergroundPlan(rx,rz);
        if(!plan||plan->kind==UndergroundKind::Mineshaft||plan->rooms.empty())continue;
        const auto c=relicChamber(*plan,w.seed());if(!inside(pos,c.center)||!ready(w,c))continue;
        discover(std::uint8_t(4+unsigned(plan->kind)),c.center);discover(8,c.center);auto s=progress(c);if(!s)continue;
        if(!(s->flags&1)){
            if(c.kind==RelicPuzzle::Routing){const auto gate=c.control(1);if(blockType(w.getBlock(gate.x,gate.y,gate.z))==Block::AndGate&&automationSignal(w.getBlock(gate.x,gate.y,gate.z))>0){s->flags|=1;notify("Current restored: stand clear of the red band during the trial");}}
            else if(c.kind==RelicPuzzle::Plates){for(int n:{0,2}){auto plate=c.control(n);if(blockType(w.getBlock(plate.x,plate.y,plate.z))==Block::OakPressurePlate&&std::abs(pos.y-plate.y-.0625f)<.12f&&glm::length(glm::vec2(pos.x-plate.x-.5f,pos.z-plate.z-.5f))<.65f)s->sequence|=n==0?1:2;}
                if(s->sequence==3){s->flags|=1;notify("Both plates attuned: pulse hall trial begins / red band is unsafe");}}
        }
        if(!(s->flags&1)||s->flags&4)continue;
        if(c.kind==RelicPuzzle::Sequence){
            if(!(s->flags&2)){
                std::array<glm::ivec3,2> spawn{c.center+glm::ivec3(-4,1,0),c.center+glm::ivec3(4,1,0)};
                if(mobs.spawnRelicAmbush(regionId(rx,rz),spawn,w)){s->flags|=2;notify("Ruins Ambush: two guards / retreat is allowed");}
            } else if(!mobs.relicEncounterAlive(regionId(rx,rz))){s->flags|=4;open(w,c);notify("Ambush defeated. Use an Ancient Seal on the vault (key retained)");}
        } else {
            const auto source=c.center+glm::ivec3(0,3,-6),lamp=c.center+glm::ivec3(0,3,-5);
            if(blockType(w.getBlock(source.x,source.y,source.z))!=Block::Lever||blockType(w.getBlock(lamp.x,lamp.y,lamp.z))!=Block::PowerLamp)continue;
            s->flags|=2;
            // A ten-second, finite hazard challenge; only occupied, fully loaded
            // chambers advance. Red floor telegraphs each strike for 1.5 seconds.
            if(s->trialTicks%25==0)notify("Pulse charging: leave the red floor band");
            const auto current=w.getBlock(source.x,source.y,source.z);
            const auto signal=s->trialTicks%25<15?blockState(Block::Lever,8):Block::Lever;
            // Lit lamp warns before the strike; it goes dark as the band fires. Two changes
            // per pulse use the existing bounded power/block-light systems.
            if(current!=signal)w.setBlock(source.x,source.y,source.z,signal);
            ++s->trialTicks;
            if(s->trialTicks%25==15&&!p.isCreative()){
                // The hazard occupies exactly the nine marked floor cells;
                // unmarked tiles beside or beyond the stripe are safe.
                const auto cell=glm::ivec3(glm::floor(pos));
                if(pos.y>=c.center.y+.95f&&pos.y<=c.center.y+1.15f&&cell.z==c.center.z&&std::abs(cell.x-c.center.x)<=4)p.damage(3);
            }
            if(s->trialTicks>=100){s->flags|=4;open(w,c);notify("Pulse trial survived. Vault unlocked with a reusable Ancient Seal");}
        }
    }
}
bool Adventure::useTool(World& w,Player& p,Inventory& inv,bool off,bool creative,std::string& message,bool echo,bool* newSurvey){
    if(newSurvey)*newSurvey=false;
    const auto held=off?inv.equipment(4):inv.selectedStack();if(held.item!=Item::SurveyorCompass&&held.item!=Item::EchoLantern)return false;
    auto& cooldown=held.item==Item::SurveyorCompass?compassCooldown_:lanternCooldown_;
    if(cooldown){message="Relic tool recharging: "+std::to_string((cooldown+9)/10)+"s";return true;}
    auto wear=[&]{if(creative)return;auto replacement=held;if(--replacement.durability<=0)replacement.clear();inv.replaceHeld(off,held,replacement);};
    if(held.item==Item::SurveyorCompass){
        auto sites=w.nearbyAdventureSites(p.position(),768);float distance=769;std::optional<AdventureDiscovery> best;
        for(const auto& site:sites){bool known=false;for(const auto& d:discoveries_){
                if(d.kind==site.kind&&d.position==site.position){known=true;break;}
                if(site.kind>=4&&site.kind<=6&&d.kind>=4&&d.kind<=8&&d.kind!=7&&
                   adventureFloorDiv(d.position.x,256)==adventureFloorDiv(site.position.x,256)&&adventureFloorDiv(d.position.z,256)==adventureFloorDiv(site.position.z,256)){known=true;break;}
            }if(known)continue;
            const auto delta=glm::vec3(site.position)-p.position();const float length=glm::length(glm::vec2(delta.x,delta.z));if(length<distance){distance=length;best=site;}}
        if(!best){cooldown=20;message="No unvisited site in surveyed regional plans. Explore farther; no charge spent";return true;}
        if(newSurvey)*newSurvey=true;surveyTarget=best->position;const auto delta=glm::vec3(*surveyTarget)-p.position();
        const char* direction=std::abs(delta.x)>std::abs(delta.z)?(delta.x>0?"EAST":"WEST"):(delta.z>0?"SOUTH":"NORTH");
        message=std::string("Surveyor: ")+direction+" ~"+std::to_string(int(distance/16)*16)+" blocks / navigation waypoint";cooldown=100;wear();
    } else {
        if(echo){
            constexpr glm::ivec3 directions[]={{0,0,-1},{1,0,0},{0,0,1},{-1,0,0}};
            constexpr const char* names[]={"NORTH","EAST","SOUTH","WEST"};
            const auto origin=glm::ivec3(glm::floor(p.position()));int best=-1,bestDistance=0,bestScore=1000;
            // Four loaded-only foot-height probes, twelve cells each. Report
            // supported air openings; no ore/material identities are inspected.
            for(int n=0;n<4;++n){int run=0;bool wall=false;
                for(int i=1;i<=12;++i){const auto q=origin+directions[n]*i;
                    if(q.y<=w.minimumY()||q.y+1>=w.maximumY()||!w.hasLoadedChunkAt(q.x,q.z))break;
                    const auto feet=w.getBlock(q.x,q.y,q.z),head=w.getBlock(q.x,q.y+1,q.z);
                    if(feet==Block::Air&&head==Block::Air&&isSolid(w.getBlock(q.x,q.y-1,q.z)))++run;
                    else {wall=wall||isSolid(feet)||isSolid(head);run=0;}
                    if(run>=3){const int distance=i-2,score=distance+(wall?0:32);if(score<bestScore){best=n;bestDistance=distance;bestScore=score;}break;}
                }
            }
            cooldown=20;if(best<0){message="No supported opening in loaded terrain (12 blocks); no charge spent";return true;}
            wear();message=std::string("Echo: supported opening ")+names[best]+" ~"+std::to_string(bestDistance)+" blocks";return true;
        }
        RayHit hit;const auto pos=p.position();const auto origin=p.cameraPosition();
        if(!w.raycast(origin,p.lookDirection(),5,hit)){message="Echo Lantern: aim at a supported floor to leave a cave light";return true;}
        const auto cell=hit.adjacent;
        if(!w.hasLoadedChunkAt(cell.x,cell.z)||w.getBlock(cell.x,cell.y,cell.z)!=Block::Air||!isSolid(w.getBlock(cell.x,cell.y-1,cell.z))){message="Lantern needs a clear cell above solid ground";return true;}
        w.setBlock(cell.x,cell.y,cell.z,Block::Torch);
        if(w.getBlock(cell.x,cell.y,cell.z)!=Block::Torch){message="Light placement rejected; charge retained";return true;}
        cooldown=20;wear();message="Echo light placed / one of 64 charges used";
    }
    return true;
}

std::vector<std::string> Adventure::journal(int category,int page)const{
    std::vector<std::string> lines;const auto maxLines=std::size_t(std::max(0,page)+1)*12;
    if(category==0){lines.push_back("BIOMES "+std::to_string(bits(biomes))+" / 35   SITES "+std::to_string(discoveries_.size()));
        for(unsigned n=0;n<35;++n)if(biomes&(std::uint64_t(1)<<n))lines.push_back(biomeDefinition(Biome(n)).name);
        static const char* names[]={"Village","Outpost","Pyramid","Landmark / ruins","Mineshaft","Dungeon","Ancient ruins","Cave passage","Relic chamber"};
        for(auto d:discoveries_){if(lines.size()>=maxLines)break;lines.push_back(std::string(names[d.kind])+"  "+std::to_string(d.position.x)+" "+std::to_string(d.position.y)+" "+std::to_string(d.position.z));}
    } else if(category==1){lines={"CHAMBERS "+std::to_string(puzzles_.size())+" / SOLVED "+std::to_string(std::count_if(puzzles_.begin(),puzzles_.end(),[](const auto& e){return e.second.flags&1;}))+" / TRIALS WON "+std::to_string(std::count_if(puzzles_.begin(),puzzles_.end(),[](const auto& e){return e.second.flags&4;})),"THREE BELLS: gold inlays show 1, 2, 3 (left to right)","DIVIDED CURRENT: restore both sides of the AND gate","PULSE HALL: walk onto both plates, then avoid red floor","A Seal is a reusable key. No Seal is consumed.","Missing mechanisms may be restored; mining can bypass walls."};
        for(const auto& e:puzzles_){if(lines.size()>=maxLines)break;lines.push_back(std::string(relicPuzzleName(e.second.kind))+(e.second.flags&8?" / OPENED":e.second.flags&4?" / TRIAL WON":e.second.flags&1?" / CHALLENGE ACTIVE":" / DISCOVERED"));}
    } else if(category==2){lines={"RELICS FOUND "+std::to_string(bits(relics))+" / 3   TOOLS CRAFTED "+std::to_string(bits(crafted&7))+" / 3   RELICS MADE "+std::to_string(bits(crafted>>3))+" / 3","Surveyor Token -> Surveyor Compass / nearby planned sites","Cavern Shard -> Echo Lantern / lights or Sneak+RMB echo","Ancient Seal -> reusable vault key / explorer pickaxe","Relics can also be crafted: see Recipe Book / all generators","Compass: 10s cooldown / Lantern: 2s cooldown","Resonant Cores: vault reward or craft from two Cavern Shards","Explorer pickaxe: Seal + Core upgrade / 1024 durability"};
    } else {for(unsigned n=0;n<4;++n)if(achievements&(1U<<n))lines.push_back("Biome Explorer: "+std::to_string(std::array<int,4>{1,5,15,35}[n]));if(achievements&16)lines.push_back("Relic Collector: all three relics");if(lines.empty())lines.push_back("Explore to uncover achievements. Rewards are granted once.");}
    const auto start=std::min(lines.size(),std::size_t(std::max(0,page))*12);return {lines.begin()+start,lines.begin()+std::min(lines.size(),start+12)};
}
bool Adventure::write(std::ostream& out)const{
    if(discoveries_.size()>MaxDiscoveries||puzzles_.size()>MaxPuzzles||(biomes>>35)||relics>7||crafted>63||achievements>31||compassCooldown_>100||lanternCooldown_>20)return false;
    if(!biomes&&!relics&&!crafted&&!achievements&&discoveries_.empty()&&puzzles_.empty()&&!compassCooldown_&&!lanternCooldown_)return true;
    out.write("ERL1",4);put(out,biomes);put(out,relics);put(out,crafted);put(out,achievements);put(out,compassCooldown_);put(out,lanternCooldown_);
    put(out,std::uint32_t(discoveries_.size()));for(auto d:discoveries_){if(d.kind>8||!position(d.position))return false;put(out,d.kind);put(out,d.position.x);put(out,d.position.y);put(out,d.position.z);}
    put(out,std::uint32_t(puzzles_.size()));for(auto [id,s]:puzzles_){if(!validProgress(id,s))return false;put(out,id);put(out,s.center.x);put(out,s.center.y);put(out,s.center.z);put(out,s.kind);put(out,s.sequence);put(out,s.flags);put(out,s.trialTicks);}
    return bool(out);
}
bool Adventure::read(std::istream& in){
    Adventure a;char magic[4];std::uint32_t n=0;in.read(magic,4);
    if(!in||std::memcmp(magic,"ERL1",4)||!get(in,a.biomes)||a.biomes>>35||!get(in,a.relics)||a.relics>7||!get(in,a.crafted)||a.crafted>63||!get(in,a.achievements)||a.achievements>31||!get(in,a.compassCooldown_)||a.compassCooldown_>100||!get(in,a.lanternCooldown_)||a.lanternCooldown_>20||!get(in,n)||n>MaxDiscoveries)return false;
    for(unsigned i=0;i<n;++i){AdventureDiscovery d;if(!get(in,d.kind)||!get(in,d.position.x)||!get(in,d.position.y)||!get(in,d.position.z)||!a.discover(d.kind,d.position))return false;}
    if(!get(in,n)||n>MaxPuzzles)return false;
    for(unsigned i=0;i<n;++i){std::uint64_t id;RelicProgress s;
        if(!get(in,id)||!get(in,s.center.x)||!get(in,s.center.y)||!get(in,s.center.z)||!get(in,s.kind)||!get(in,s.sequence)||!get(in,s.flags)||!get(in,s.trialTicks)||!validProgress(id,s)||!a.puzzles_.emplace(id,s).second)return false;
    }
    a.notice_.clear();*this=std::move(a);return true;
}

std::shared_ptr<const UndergroundPlan> UndergroundStructures::cached(int x,int z)const{std::lock_guard<std::mutex> lock(mutex_);auto it=cache_.find(std::int64_t(Adventure::regionId(x,z)));return it==cache_.end()?nullptr:it->second;}
std::shared_ptr<const UndergroundPlan> World::cachedUndergroundPlan(int x,int z)const{return (generationVersion_>=24?underground24_:generationVersion_>=23?underground23_:generationVersion_>=21?underground21_:generationVersion_>=20?underground20_:underground_).cached(x,z);}
std::vector<AdventureDiscovery> World::nearbyAdventureSites(glm::vec3 pos,float range)const{
    std::vector<AdventureDiscovery> result;range=std::clamp(range,0.f,768.f);
    const int cx=floorDiv(int(std::floor(pos.x)),16),cz=floorDiv(int(std::floor(pos.z)),16),radius=range<=16?1:std::min(48,int(range/16)+1);
    // Ordered lower_bound visits at most the bounded resident marker index.
    if(range<=16){
        for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){auto it=adventureSites_.find(chunkKey(cx+x,cz+z));if(it==adventureSites_.end())continue;for(auto d:it->second)if(d.kind<8&&glm::distance(glm::vec3(d.position),pos)<=range&&hasLoadedChunkAt(d.position.x,d.position.z))result.push_back(d);}
        const int rx=floorDiv(int(std::floor(pos.x)),256),rz=floorDiv(int(std::floor(pos.z)),256);
        for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x)if(auto plan=cachedUndergroundPlan(rx+x,rz+z)){auto p=plan->entrance;if(glm::distance(glm::vec3(p.x,p.y,p.z),pos)<=range&&hasLoadedChunkAt(p.x,p.z))result.push_back({std::uint8_t(4+unsigned(plan->kind)),{p.x,p.y,p.z}});}
        return result;
    }
    for(const auto& group:adventureSites_){const auto x=std::int32_t(std::uint64_t(group.first)>>32),z=std::int32_t(group.first);if(std::abs(x-cx)>radius||std::abs(z-cz)>radius)continue;
        for(auto d:group.second)if(d.kind<8&&glm::distance(glm::vec3(d.position),pos)<=range&&hasLoadedChunkAt(d.position.x,d.position.z))result.push_back(d);}
    if(range>16){const int rx=floorDiv(int(std::floor(pos.x)),256),rz=floorDiv(int(std::floor(pos.z)),256);for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)if(auto plan=cachedUndergroundPlan(rx+x,rz+z)){auto p=plan->entrance;if(glm::length(glm::vec2(p.x-pos.x,p.z-pos.z))<=range)result.push_back({std::uint8_t(4+unsigned(plan->kind)),{p.x,p.y,p.z}});}}
    return result;
}

bool SurvivalWorld::spawnRelicAmbush(std::uint64_t identity,const std::array<glm::ivec3,2>& positions,const World& w){
    const auto tag=identity^0xa719000000000000ULL;
    unsigned existing=0;for(auto p:positions)existing+=spawnedStructureMarkers_.count(structureMobMarkerId(p.x,p.y,p.z,true));
    if(existing)return existing==positions.size();
    if(animals_.size()+2>SaveValidation::MaxMobs||spawnedStructureMarkers_.size()+2>SaveValidation::MaxMarkers)return false;
    for(auto p:positions)if(!w.hasLoadedChunkAt(p.x,p.z)||!isSolid(w.getBlock(p.x,p.y-1,p.z))||isSolid(w.getBlock(p.x,p.y,p.z))||isSolid(w.getBlock(p.x,p.y+1,p.z)))return false;
    auto markers=spawnedStructureMarkers_;for(auto p:positions)markers.insert(structureMobMarkerId(p.x,p.y,p.z,true));
    animals_.reserve(animals_.size()+2);spawnedStructureMarkers_.swap(markers);
    for(auto p:positions){Animal a;a.type=AnimalType::Pillager;a.position=glm::vec3(p)+glm::vec3(.5f,.01f,.5f);a.home=a.position;a.health=14;a.persistent=true;a.thinkTimer=.5f;a.relicEncounter=tag;animals_.push_back(a);}
    return true;
}
bool SurvivalWorld::relicEncounterAlive(std::uint64_t identity)const{
    const auto tag=identity^0xa719000000000000ULL;
    return std::any_of(animals_.begin(),animals_.end(),[&](const Animal& a){return a.relicEncounter==tag&&a.health>0&&a.deathTimer<=0;});
}
bool SurvivalWorld::relicEncountersValid(const Adventure& a)const{
    for(const auto& mob:animals_)if(mob.relicEncounter&&mob.health>0&&mob.deathTimer<=0){
        if(!validRelicEncounterTag(mob.relicEncounter))return false;
        auto it=a.puzzles().find(mob.relicEncounter^0xa719000000000000ULL);
        if(it==a.puzzles().end()||it->second.kind!=RelicPuzzle::Sequence||!(it->second.flags&2)||(it->second.flags&4))return false;
    }
    return true;
}
void World::retainGeneratedAutomation(int x,int z){
    if(generationVersion_<24)return;
    auto c=findChunk(x,z);if(!c||c->automationCells.empty())return;
    // AUT1 validates ownership against saved terrain. Generated mechanisms need
    // their canonical state retained even before the first electrical change.
    auto& changed=edits_[chunkKey(x,z)];for(auto i:c->automationCells)changed.emplace(i,c->blocks[i]);
}
