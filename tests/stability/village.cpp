#include <windows.h>
#include <psapi.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Renderer.h"
#include "Transportation.h"
#include "Villages.h"
#include "VillageUi.h"
#include "UiLayout.h"
#include "Screenshot.h"
#include "Weather.h"
#include "Sound.h"
#include "SaveFile.h"
#include "Persistence.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <chrono>
#include <thread>
class VillageTest {
 static inline unsigned checks=0;
 static void check(bool b,const char* name){++checks;std::cout<<"village "<<name<<" passed="<<b<<std::endl;if(!b)throw std::runtime_error(name);}
 static auto terrain(){return [](int,int){return StructureTerrain{80,StructureBiome::Plains,0,48,320,true,false};};}
 static std::shared_ptr<const StructurePlan> plan(World& w){for(int z=-6;z<=6;++z)for(int x=-6;x<=6;++x){w.structures_.majorPlanForRegion(x,z,terrain());auto p=w.structures_.peekMajorPlan(x,z);if(p&&!p->pieces.empty()&&(p->kind==StructureKind::PlainsVillage||p->kind==StructureKind::DesertVillage)&&p->pieces.size()>6)return p;}throw std::runtime_error("No village fixture");}
 static void flat(World& w,const StructurePlan& p){int cx=adventureFloorDiv(p.pieces[0].x,16),cz=adventureFloorDiv(p.pieces[0].z,16);for(int z=cz-4;z<=cz+4;++z)for(int x=cx-4;x<=cx+4;++x){World::GeneratedChunk d;d.x=x;d.z=z;d.blocks.assign(16*16*384,Block::Air);for(int y=-64;y<=80;++y)for(int Z=0;Z<16;++Z)for(int X=0;X<16;++X)d.blocks[((y+64)*16+Z)*16+X]=y==80?Block::Grass:Block::Stone;w.chunks_[w.chunkKey(x,z)]=std::make_unique<Chunk>(x,z,std::move(d.blocks),-64,320);}
  for(auto r:p.roads)w.setBlockInternal(r.x,r.y,r.z,r.block,true,false);w.edits_.clear();
 }
 static void residency(){World w(991,24);auto p=plan(w);flat(w,*p);auto pc=p->pieces.front();glm::vec3 pos(pc.x+.5f,81.01f,pc.z+.5f);SurvivalWorld mobs(991);Player player(pos);Inventory inventory;mobs.summonMob("villager",pos);auto chunks=std::move(w.chunks_);auto plans=std::move(w.structures_.majorCache_);mobs.update(1.f/60,w,player,inventory,1);bool deferred=w.villages.people().empty()&&!mobs.animals_.front().villagerId;w.chunks_=std::move(chunks);w.structures_.majorCache_=std::move(plans);check(deferred,"legacy_NPC_defers_association_until_terrain_ready");mobs.update(1.f/60,w,player,inventory,1);auto id=mobs.animals_.front().villagerId;check(id&&w.villages.npc(id)->village,"legacy_NPC_associates_after_plan_and_terrain_ready");}
 static bool same(const Inventory& a,const Inventory& b){for(int i=0;i<36;++i){auto x=a.slot(i),y=b.slot(i);if(x.item!=y.item||x.count!=y.count||x.durability!=y.durability||x.identity!=y.identity||a.hotbarLocked(i)!=b.hotbarLocked(i))return false;}for(int i=0;i<5;++i){auto x=a.equipment(i),y=b.equipment(i);if(x.item!=y.item||x.count!=y.count||x.identity!=y.identity||x.durability!=y.durability)return false;}auto x=a.cursorStack(),y=b.cursorStack();return x.item==y.item&&x.count==y.count&&x.durability==y.durability&&x.identity==y.identity;}
 static void unit(const std::filesystem::path& out){residency();World w(991,24);auto p=plan(w);flat(w,*p);auto c=glm::ivec3(p->pieces[0].x,81,p->pieces[0].z);auto id=w.villages.bind(w,glm::vec3(c));check(id!=0,"bind_existing_plan");auto n=w.villages.npc(id);auto v=w.villages.village(n->village);check(v&&v->regionX==p->regionX&&v->regionZ==p->regionZ,"canonical_village_identity");auto vid=v->id;
  World other(991,24);auto p2=plan(other);auto id2=other.villages.bind(other,glm::vec3(c));check(other.villages.npc(id2)->village==vid&&other.villages.npc(id2)->job==n->job,"stable_assignment_independent_runtime");
  Inventory inv;std::string msg;Player player(glm::vec3(c)+glm::vec3(15,1,15));SurvivalWorld mobs(991);
  for(unsigned job=0;job<4;++job){n->job=VillageJob(job);for(int row=0;row<4;++row){n->stock[row]=4;auto o=w.villages.pricedOffer(id,row);inv.clear();inv.add(o.input,o.cost+1);inv.equipment_[4]={Item::Shield,1,33};inv.cursor_={Item::FilledMap,1,0,17};auto cursor=inv.cursor_;check(w.villages.trade(id,row,inv,msg),"trade_success_all_professions");check(inv.count(o.output)==o.count&&inv.count(o.input)==1&&n->stock[row]==3,"trade_conservation_and_stock");check(inv.cursor_.identity==cursor.identity&&inv.equipment(4).durability==33,"protected_metadata_owners");}}
  check(v->tradeReputation==10&&v->reputation==10,"trade_reputation_lifetime_cap");
  n->job=VillageJob::Farmer;v->reputation=0;auto o=w.villages.pricedOffer(id,0);inv.clear();auto before=inv;auto stock=n->stock;check(!w.villages.trade(id,0,inv,msg)&&same(inv,before)&&stock==n->stock,"failed_trade_no_mutation");
  inv.add(Item::Wheat,20);inv.toggleHotbarLock(0);before=inv;check(!w.villages.trade(id,0,inv,msg)&&same(inv,before),"locked_inputs_retained");inv.clear();for(auto& s:inv.slots_)s={Item::Stone,64};inv.slots_[10]={Item::Wheat,21};before=inv;check(!w.villages.trade(id,0,inv,msg)&&same(inv,before),"full_output_no_consumption");
  inv.clear();inv.add(Item::Wheat,64);n->stock[0]=0;before=inv;check(!w.villages.trade(id,0,inv,msg)&&same(inv,before),"exhausted_stock");
  v->seconds=300;v->supply=0;n->restockAt=300;w.villages.destination(id,100,false,false);check(n->stock[0]==0,"no_free_restock_without_supply");v->seconds=600;v->supply=8;w.villages.destination(id,100,false,false);check(n->stock[0]==1&&v->supply==4,"limited_supply_restock");w.villages.destination(id,100,false,false);check(n->stock[0]==1,"restock_not_repeated");v->seconds=10000;w.villages.destination(id,100,false,false);check(n->stock[0]==2&&v->supply==0,"no_catchup_restock_explosion");
  w.villages.destination(id,800,false,false);check(n->routine==2,"night_returns_home");w.villages.destination(id,520,false,false);check(n->routine==1,"evening_meeting");w.villages.destination(id,100,true,false);check(n->routine==3,"weather_shelter");w.villages.destination(id,100,false,true);check(n->routine==4&&v->threatUntil>v->seconds,"witnessed_danger");w.villages.destination(id,100,false,false);
  v->requests={VillageRequest{0,0,0},VillageRequest{1,0,0},VillageRequest{3,0,0}};
  for(int row=0;row<3;++row)check(w.villages.request(id,row,0,inv,player,msg),"accept_request");
  inv.clear();inv.add(Item::Bread,12);check(w.villages.request(id,0,1,inv,player,msg),"delivery_reward");auto xp=player.experience();before=inv;check(!w.villages.request(id,0,1,inv,player,msg)&&player.experience()==xp&&same(inv,before),"exactly_once_request");
  inv.clear();inv.add(Item::Seeds,24);check(w.villages.request(id,1,1,inv,player,msg),"farming_seed_delivery");check(!w.villages.request(id,2,1,inv,player,msg),"rescue_requires_help");w.villages.assistance(glm::vec3(c));check(w.villages.request(id,2,1,inv,player,msg),"witnessed_assistance_reward");
  inv.clear();inv.add(Item::Cobblestone,8);check(w.villages.contribute(id,0,inv,msg)&&v->funding[0][0]==8,"partial_project_funding");before=inv;Inventory full;full.clear();for(auto& stack:full.slots_)stack={Item::Stone,64};check(!w.villages.withdraw(id,0,full,msg)&&v->funding[0][0]==8,"full_refund_keeps_escrow");check(w.villages.withdraw(id,0,inv,msg)&&v->funding[0][0]==0&&inv.count(Item::Cobblestone)==8,"refund_once_conserves_materials");check(w.villages.contribute(id,0,inv,msg),"refunded_materials_can_be_repledged");before=inv;check(!w.villages.construct(id,0,w,mobs,player,msg)&&v->projects==0&&same(inv,before),"incomplete_project_rejected");inv.add(Item::Cobblestone,8);inv.add(Item::Log,4);check(w.villages.contribute(id,0,inv,msg),"complete_project_funding");check(w.villages.construct(id,0,w,mobs,player,msg)&&v->projects==1,"explicit_road_improvement");xp=player.experience();check(!w.villages.construct(id,0,w,mobs,player,msg)&&player.experience()==xp,"project_never_repeats");
  v->rotation=1;v->requests[0]={2,1,0};check(w.villages.request(id,0,1,inv,player,msg),"building_request_links_public_works");
  inv.clear();inv.add(Item::Log,8);inv.add(Item::IronIngot,4);check(w.villages.contribute(id,1,inv,msg),"stall_funding");
  auto savedChunks=std::move(w.chunks_);check(!w.villages.construct(id,1,w,mobs,player,msg)&&v->funding[1][0]==8,"unloaded_project_keeps_funding");w.chunks_=std::move(savedChunks);bool built=w.villages.construct(id,1,w,mobs,player,msg);if(!built)std::cout<<"Construction diagnostic: "<<msg<<std::endl;check(built&&(v->projects&2),"community_stall_visible_edits");
  n->stock[4]=2;inv.clear();auto unlock=w.villages.pricedOffer(id,4);inv.add(unlock.input,unlock.cost);check(w.villages.trade(id,4,inv,msg),"stall_unlocks_useful_offer");
  const auto rep=v->reputation;w.villages.attacked(id,false);check(v->reputation<rep,"negative_history_recorded");
  check(w.villages.valid(991,24),"bounded_state_invariants");std::ostringstream encoded(std::ios::binary);check(w.villages.write(encoded,991,24),"village_extension_encode");auto bytes=encoded.str();Villages loaded;std::istringstream input(bytes,std::ios::binary);check(loaded.read(input,991,24)&&loaded.village(vid)->reputation==v->reputation&&loaded.npc(id)->stock==n->stock,"village_extension_roundtrip");
  auto malformed=[&](std::string bad){Villages retained=loaded;std::istringstream f(bad,std::ios::binary);check(!retained.read(f,991,24)&&retained.npc(id)->stock==loaded.npc(id)->stock,"malformed_extension_no_partial_commit");};
  auto bad=bytes;bad[4]=char(255);bad[5]=char(255);malformed(bad);
  bad=bytes;bad[4]=2;bad.insert(8+116,bytes.substr(8,116));malformed(bad);
  bad=bytes;bad[8+64]=char(255);bad[8+65]=char(255);malformed(bad); // invalid supply
  bad=bytes;bad[8+116]=char(255);bad[8+117]=char(255);malformed(bad);
  bad=bytes;bad[8+116+4+41+8]=char(255);malformed(bad); // invalid stock
  auto sec=v->seconds;auto absent=std::move(w.chunks_);w.villages.advance(1.1f,w);check(v->seconds==sec,"unloaded_village_clock_suspended");w.chunks_=std::move(absent);
  auto traveler=w.villages.bind(w,{29000,81,29000});check(traveler&&!w.villages.npc(traveler)->village,"summoned_independent_villager_valid");
  for(std::size_t cut=0;cut<bytes.size();++cut){Villages untouched=loaded;std::istringstream shortFile(bytes.substr(0,cut),std::ios::binary);check(!untouched.read(shortFile,991,24)&&untouched.npc(id)->stock==n->stock,"truncated_no_partial_commit");}
  if(Persistence::enabled()){
   auto path=(out/"village.vxw").string();auto capture=w.captureSaveWorld(path,player.position());n->stock[0]=0;check(capture(),"immutable_village_snapshot");World after(991,1);glm::vec3 pos;check(after.loadWorld(path,pos)&&after.villages.village(vid)->projects==3&&after.villages.npc(id)->stock[0]==loaded.npc(id)->stock[0],"world_village_codec_and_legacy_version");
   mobs.spawnStructureMob(c,false);auto mid=mobs.villagerIdentity(0,w);check(mid&&mobs.villageCheckpoint(w),"stable_npc_binding");auto mobpath=(out/"villagers.vxm").string();check(mobs.save(mobpath,991),"npc_id_encode");SurvivalWorld again(991);check(again.load(mobpath,991)&&again.villageCheckpoint(w,true)&&again.animals_[0].villagerId==mid,"npc_id_decode_and_link");again.animals_[0].health=0;again.animals_[0].deathTimer=.5f;check(again.villageCheckpoint(w)&&w.villages.npc(mid)->dead,"dead_npc_tombstone");
  }
  {
   SurvivalWorld reorder(991);SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType::Villager;a.position=a.home={29500,81,29500};a.health=20;a.villagerId=w.villages.bind(w,a.home);auto first=a.villagerId;reorder.animals_.push_back(a);a.position=a.home={29502,81,29500};a.villagerId=w.villages.bind(w,a.home);auto second=a.villagerId;reorder.animals_.push_back(a);
   check(reorder.villagerAvailable(first,{29500,81,29500}),"identity_cache_initial");std::swap(reorder.animals_[0],reorder.animals_[1]);check(reorder.villagerAvailable(first,{29500,81,29500}),"identity_survives_vector_reorder");reorder.animals_.erase(reorder.animals_.begin()+1);check(!reorder.villagerAvailable(first,{29500,81,29500})&&reorder.villagerAvailable(second,{29502,81,29500}),"identity_survives_vector_erase");
   auto primaryRep=v->reputation;bool another=false;for(int z=-6;z<=6&&!another;++z)for(int x=-6;x<=6&&!another;++x){auto q=w.structures_.majorPlanForRegion(x,z,terrain());if(q.pieces.empty()||(q.kind!=StructureKind::PlainsVillage&&q.kind!=StructureKind::DesertVillage)||(q.regionX==v->regionX&&q.regionZ==v->regionZ))continue;auto pc=q.pieces.front();auto foreign=w.villages.bind(w,{float(pc.x),float(pc.y+1),float(pc.z)});auto person=w.villages.npc(foreign);if(person&&person->village&&person->village!=vid){w.villages.attacked(foreign,false);check(w.villages.village(person->village)->reputation<0&&v->reputation==primaryRep,"multiple_villages_independent_reputation");another=true;}}
   check(another,"second_canonical_plan_association");
  }
  {
   glm::ivec3 base=c+glm::ivec3(48,0,48);auto clear=[&]{for(int z=-2;z<=5;++z)for(int x=-2;x<=5;++x)for(int y=0;y<4;++y)w.setBlockInternal(base.x+x,base.y+y,base.z+z,Block::Air,false,false);};
   auto walk=[&](Block obstacle,bool door=false){clear();w.setBlockInternal(base.x,base.y,base.z+1,obstacle,false,false);if(door)w.setBlockInternal(base.x,base.y+1,base.z+1,doorBlock(0,true,false,true),false,false);SurvivalWorld::Animal person;person.type=SurvivalWorld::AnimalType::Villager;person.position=person.home=glm::vec3(base)+glm::vec3(.5f,.01f,.5f);person.health=20;person.grounded=true;for(int frame=0;frame<360;++frame){person.stepJumpCooldown=std::max(0.f,person.stepJumpCooldown-1.f/60);glm::vec3 next;mobs.moveVillager(person,w,{0,0,.75f},1.f/60,next);person.position=next;check(!w.aabbIntersectsSolid(next-glm::vec3(.299f,0,.299f),next+glm::vec3(.299f,1.599f,.299f)),"villager_sweep_never_enters_solid");}return person.position;};
   check(walk(doorBlock(0,false,false,true),true).z>base.z+3,"villager_crosses_open_door_leaf");check(walk(Block::Stone).z>base.z+3,"villager_steps_full_block");check(walk(Block::OakSlab).z>base.z+3,"villager_steps_half_slab");check(walk(Block::OakFence).z<base.z+1.2f,"villager_respects_fence");
   clear();w.setBlockInternal(base.x,base.y,base.z+1,Block::Stone,false,false);SurvivalWorld::Animal person;person.type=SurvivalWorld::AnimalType::Villager;person.position=person.home=glm::vec3(base)+glm::vec3(.5f,.01f,.5f);person.heading={0,1};person.health=20;person.grounded=true;for(int frame=0;frame<360;++frame){person.thinkTimer=10;mobs.updateAnimal(person,1.f/60,w,player,inv,1);}check(person.position.z>base.z+3,"whole_villager_update_preserves_step_jump_heading");
  }
  for(auto size:{glm::ivec2(1280,720),glm::ivec2(1920,1080),glm::ivec2(2560,1440)})for(int scale=0;scale<4;++scale){float s=UiLayout::interfaceScale(size.x,size.y,scale);VillageUiLayout l(int(size.x/s),int(size.y/s));for(int row=0;row<5;++row)check(l.row(l.x+24,l.y+108+(row+.5f)*l.rowHeight,5)==row,"scaled_mouse_rows");}
 }
 #define VILLAGE_NEW_BUILD
 #include "village_native.inc"
 #include "../village_benchmark.inc"
public:
 static int run(int argc,char** argv){auto out=std::filesystem::absolute(argc>2?argv[2]:"village-validation");std::filesystem::create_directories(out);Game g;char app[]="VillageFixture";char* args[]={app};check(g.initialize(1,args),"initialize");glfwHideWindow(g.window_);if(std::string(argv[1])=="--village-residency")residency();else if(std::string(argv[1])=="--village-benchmark")benchmark(g,out,argc,argv);else if(std::string(argv[1])=="--village-route")route(g,out,argc>3?std::stoi(argv[3]):30,argc>4?std::stoi(argv[4]):8);else unit(out);g.saveOnExit_=false;g.shutdown();std::ofstream(out/"results.json")<<"{\"assertions\":"<<checks<<",\"passed\":true}\n";std::cout<<"Village checks passed: "<<checks<<std::endl;return 0;}
};
int runVillageTests(int argc,char** argv){return VillageTest::run(argc,argv);}
