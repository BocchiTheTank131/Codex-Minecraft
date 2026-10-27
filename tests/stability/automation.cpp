#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "World.h"
#include "Player.h"
#include "Sound.h"
#include "Renderer.h"
#include "Transportation.h"
#include "SaveFile.h"
#include "WorldLibrary.h"
#include "Screenshot.h"
#include "Persistence.h"
#include "Farming.h"
#include "SaveValidation.h"
double fixtureWorkingSetMb();
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <stdexcept>

class AutomationTest {
 static inline int assertions=0;
 static void check(bool ok,const std::string& name){++assertions;std::cout<<"automation "<<name<<" passed="<<ok<<std::endl;if(!ok)throw std::runtime_error(name);}
 static void empty(World& w,int radius=3){
  for(int z=-radius;z<=radius;++z)for(int x=-radius;x<=radius;++x){std::vector<Block> data(16*16*384,Block::Air);for(int dz=0;dz<16;++dz)for(int dx=0;dx<16;++dx)data[((299+64)*16+dz)*16+dx]=Block::Stone;auto c=std::make_unique<Chunk>(x,z,std::move(data),-64,320);c->meshIdentity=std::uint64_t((x+radius+1)*100+z+radius+1);w.chunks_.emplace(w.chunkKey(x,z),std::move(c));}
 }
 static Block get(const World& w,glm::ivec3 p){return w.getBlock(p.x,p.y,p.z);}
 static void put(World& w,glm::ivec3 p,Block b){check(w.setBlockInternal(p.x,p.y,p.z,b,true,false)||get(w,p)==b,"place_"+std::to_string(int(blockType(b))));}
 static void ticks(World& w,SurvivalWorld& s,int count,const Player* player=nullptr,float light=1){for(int i=0;i<count;++i)w.automation.update(.1f,w,s,player,light);}
 static int count(const ChestData* c,Item item){int n=0;if(c)for(auto s:c->slots)if(s.item==item)n+=s.count;return n;}
 static void circuits();
 static void transportation();
 static void machinery();
 static void persistence();
 static void production(Game&);
 static void stabilization(Game&,const std::filesystem::path&);
 static void benchmark(const std::filesystem::path&);
 static void sparse(const std::filesystem::path&);
 static void systems(const std::filesystem::path&);
 static void visual(Game&,const std::filesystem::path&);
public:static int run(int,char**);
 static void setupTraversal(Game&,int);
 static void driveTraversal(Game&,double);
 static inline std::vector<glm::ivec3> traversalSources;
 static inline int traversalPulse=-1;
};
int runAutomationTests(int argc,char** argv){return AutomationTest::run(argc,argv);}
int AutomationTest::run(int argc,char** argv){Game g;char app[]="automation-validation";char* args[]={app};check(g.initialize(1,args),"initialize");glfwHideWindow(g.window_);const auto out=std::filesystem::absolute(argc>2?argv[2]:"evidence");std::filesystem::create_directories(out);
 const std::string mode=argc>1?argv[1]:"--automation";
 if(mode=="--automation-stabilization"){stabilization(g,out);return 0;}
 if(mode=="--automation-sparse"){sparse(out);return 0;}
 if(mode=="--automation-systems"){systems(out);return 0;}
 if(mode=="--automation-demo"){check(!Persistence::enabled(),"demo_policy_disabled");circuits();transportation();machinery();g.shutdown();for(auto file:std::filesystem::recursive_directory_iterator(".")){auto name=file.path().filename().string();check(file.path().extension()!=".vxw"&&file.path().extension()!=".vxi"&&file.path().extension()!=".vxm"&&file.path().extension()!=".vps"&&file.path().extension()!=".vtn"&&name!="checkpoint.manifest"&&name!=".session.lock","demo_no_persistent_components");}std::cout<<"Demo automation assertions="<<assertions<<std::endl;return 0;}
 if(mode=="--automation-benchmark"){benchmark(out);return 0;}
 if(mode=="--automation-visual"){visual(g,out);return 0;}
 circuits();transportation();machinery();persistence();production(g);std::string report;const bool recipes=Inventory::runCraftingSelfTest(report);check(recipes,"production_recipes_"+report);
 check(glGetError()==GL_NO_ERROR,"no_gl_errors");std::cout<<"Automation assertions="<<assertions<<'\n';return 0;
}
void AutomationTest::circuits(){
 World w(78,22);empty(w);SurvivalWorld s(78);auto src=glm::ivec3(0,300,0);put(w,src,blockState(Block::Lever,8));
 for(int x=1;x<=17;++x)put(w,{x,300,0},Block::PowerWire);for(int z=1;z<=5;++z)put(w,{4,300,z},Block::PowerWire);
 ticks(w,s,24);for(int x=1;x<=17;++x)check(automationSignal(get(w,{x,300,0}))==std::max(0,16-x),"wire_attenuation_"+std::to_string(x));check(automationSignal(get(w,{4,300,5}))==7,"branch_attenuation");
 put(w,{3,300,-1},Block::PowerLamp);ticks(w,s,3);check(blockLightEmission(get(w,{3,300,-1}))==15,"lamp_emission");w.rebuildBlockLighting();check(w.blockLightAt(3,300,-2)>0,"lamp_existing_light_solver");
 put(w,{3,300,-2},Block::OakFenceGate);put(w,{2,300,-2},blockState(Block::Lever,8));ticks(w,s,3);check(blockProperties(get(w,{3,300,-2}))&8,"powered_gate_opens");
 w.automation.interact(w,src);ticks(w,s,40);check(automationSignal(get(w,{1,300,0}))==0&&blockLightEmission(get(w,{3,300,-1}))==0,"source_removal_drains_cycles_and_lamp");
 // Restore crossing boundary, unload and resume without treating missing chunks as Air.
 w.automation.interact(w,src);ticks(w,s,24);const int held=automationSignal(get(w,{15,300,0}));w.automation.suspendChunk(1,0);auto unloaded=std::move(w.chunks_.at(w.chunkKey(1,0)));w.chunks_.erase(w.chunkKey(1,0));w.automation.interact(w,src);ticks(w,s,40);check(automationSignal(get(w,{15,300,0}))==held,"boundary_suspends");w.chunks_.emplace(w.chunkKey(1,0),std::move(unloaded));w.automation.activateChunk(w,1,0);ticks(w,s,40);check(automationSignal(get(w,{15,300,0}))==0,"boundary_resume");
 // Gates face north: side inputs east/west, output north. No backfeed.
 for(auto gate:{Block::AndGate,Block::OrGate,Block::NotGate}){
  World a(3,22);empty(a,1);SurvivalWorld mobs(3);put(a,{5,300,5},gate);put(a,{5,300,4},Block::PowerWire);
  for(int bits=0;bits<4;++bits){put(a,{4,300,5},blockState(Block::Lever,bits&1?8:0));put(a,{6,300,5},blockState(Block::Lever,bits&2?8:0));put(a,{5,300,6},blockState(Block::Lever,bits&1?8:0));ticks(a,mobs,8);bool expected=gate==Block::AndGate?bits==3:gate==Block::OrGate?bits!=0:!(bits&1);check((automationSignal(get(a,{5,300,5}))>0)==expected,"gate_truth_table");}
 }
 World r(4,22);empty(r,1);SurvivalWorld rm(4);put(r,{5,300,5},blockState(Block::Repeater,48));ticks(r,rm,2);put(r,{5,300,6},blockState(Block::Lever,8));ticks(r,rm,1);check(automationSignal(get(r,{5,300,5}))==0,"repeater_delay_start");ticks(r,rm,3);check(automationSignal(get(r,{5,300,5}))==0,"repeater_delay_pending");ticks(r,rm,2);check(automationSignal(get(r,{5,300,5}))==15,"repeater_restores_signal");check(r.automation.interact(r,{5,300,5})&&((blockProperties(get(r,{5,300,5}))>>4)&3)==0,"repeater_delay_cycles");
 put(r,{8,300,5},Block::Observer);ticks(r,rm,2);put(r,{8,300,6},Block::Dirt);ticks(r,rm,1);check(automationSignal(get(r,{8,300,5}))==15,"observer_pulse");ticks(r,rm,3);check(automationSignal(get(r,{8,300,5}))==0,"observer_pulse_finishes");put(r,{8,300,6},Block::Stone);put(r,{8,300,6},Block::Dirt);ticks(r,rm,1);check(automationSignal(get(r,{8,300,5}))==15,"observer_rapid_return_to_same_state_still_pulses");
 put(r,{10,300,5},Block::DaylightSensor);auto* c=r.findChunk(0,0);c->setSunlightLocal(c->indexAt(10,301,5),15);ticks(r,rm,12,nullptr,1);check(automationSignal(get(r,{10,300,5}))==15,"daylight_day");ticks(r,rm,12,nullptr,0);check(automationSignal(get(r,{10,300,5}))==0,"daylight_night");
 put(r,{12,300,5},Block::OakPressurePlate);Player player({12.5f,300,5.5f});ticks(r,rm,2,&player);check(blockProperties(get(r,{12,300,5}))&8,"plate_actual_player_volume");player.teleport({12.5f,302,5.5f});ticks(r,rm,2,&player);check(!(blockProperties(get(r,{12,300,5}))&8),"plate_no_nearby_trigger");
 put(r,{11,300,10},blockState(Block::OakButton,0));check(r.automation.interact(r,{11,300,10}),"button_interaction");ticks(r,rm,3);check(automationSignal(get(r,{11,300,10}))==15,"button_pulse_holds");ticks(r,rm,3);check(automationSignal(get(r,{11,300,10}))==0,"button_finite_timeout");
 World stairs(2,22);empty(stairs,2);SurvivalWorld sm(2);put(stairs,{-2,300,-2},blockState(Block::Lever,8));put(stairs,{-1,300,-2},Block::PowerWire);put(stairs,{0,300,-2},Block::Stone);put(stairs,{0,301,-2},Block::PowerWire);put(stairs,{1,300,-2},Block::PowerWire);ticks(stairs,sm,12);check(automationSignal(get(stairs,{0,301,-2}))==14&&automationSignal(get(stairs,{1,300,-2}))==13,"negative_coordinate_up_down_connections");auto shape=buildingConnectedState(stairs,{-1,300,-2},get(stairs,{-1,300,-2}));check((blockProperties(shape)&8192)!=0,"wire_visible_vertical_connection");
 for(auto door:{Block::OakDoor,Block::OakTrapdoor,Block::BirchFenceGate,Block::DoorClosedNorthLower}){World d(6,22);empty(d,1);SurvivalWorld dm(6);put(d,{5,300,5},door);if(buildingIsDoor(door))put(d,{5,301,5},blockState(door,16));if(isDoor(door))put(d,{5,301,5},Block::DoorClosedNorthUpper);put(d,{4,300,5},blockState(Block::Lever,8));ticks(d,dm,4);auto opened=get(d,{5,300,5});check(isDoor(opened)?isDoorOpen(opened):bool(blockProperties(opened)&8),"door_power_open");put(d,{5,300,5},isDoor(door)?door:blockState(door,0));ticks(d,dm,3);check(get(d,{5,300,5})==door,"manual_override_while_power_held");d.automation.interact(d,{4,300,5});ticks(d,dm,3);check(isDoor(get(d,{5,300,5}))?!isDoorOpen(get(d,{5,300,5})):!(blockProperties(get(d,{5,300,5}))&8),"door_power_falling_closes");}
 // An intentionally oscillating inverter loop remains bounded; per-tick work never exceeds budget.
 put(r,{1,300,10},Block::NotGate);put(r,{1,300,9},Block::PowerWire);put(r,{2,300,9},Block::PowerWire);put(r,{2,300,10},Block::PowerWire);put(r,{2,300,11},Block::PowerWire);put(r,{1,300,11},Block::PowerWire);int edges=0,prior=-1;for(int i=0;i<200;++i){ticks(r,rm,1);int output=automationSignal(get(r,{1,300,10}));if(output!=prior)++edges;prior=output;check(r.automation.diagnostics().processed<=2048,"oscillator_bounded");}check(edges>6,"clock_emits_repeated_pulses");
 {World large(31,22);empty(large,4);SurvivalWorld ls(31);for(int i=0;i<3000;++i){glm::ivec3 p(i%60,300,i/60);large.setBlockInternal(p.x,p.y,p.z,i%64?Block::PowerWire:blockState(Block::Lever,8),true,false);}for(int i=0;i<80;++i){ticks(large,ls,1);check(large.automation.diagnostics().processed<=2048&&large.automation.diagnostics().changes<=512,"large_network_evaluation_and_commit_bounded");}check(large.automation.diagnostics().devices==3000,"large_network_retains_registered_nodes");}
 // Fixed time with different render steps yields the same final world states.
 for(int fps:{10,15,20,30,60,120,240}){World f(5,22);empty(f,1);SurvivalWorld fm(5);put(f,{2,300,2},blockState(Block::Lever,8));for(int x=3;x<12;++x)put(f,{x,300,2},Block::PowerWire);for(int i=0;i<fps*6;++i)f.automation.update(1.f/fps,f,fm,nullptr,1);check(automationSignal(get(f,{11,300,2}))==7,"fps_independent_"+std::to_string(fps));}
}
void AutomationTest::transportation(){
 World w(9,22);empty(w,2);SurvivalWorld s(9);put(w,{3,302,3},Block::Chest);put(w,{3,301,3},blockState(Block::Hopper,4));put(w,{3,300,3},Block::Chest);auto* source=w.chestAt({3,302,3},true);source->slots[0]={Item::FilledMap,1,0,481};source->slots[1]={Item::IronSword,1,29};source->slots[2]={Item::Stone,64};ticks(w,s,560);auto* dst=w.chestAt({3,300,3});check(count(dst,Item::Stone)==64&&count(dst,Item::FilledMap)==1&&count(dst,Item::IronSword)==1,"hopper_chain_conservation");check(dst->slots[0].identity==481&&dst->slots[1].durability==29,"hopper_metadata_identity_durability");
 put(w,{2,301,3},blockState(Block::Lever,8));source->slots[0]={Item::Diamond,3};ticks(w,s,50);check(count(source,Item::Diamond)==3,"hopper_power_lock");put(w,{2,301,3},Block::Air);ticks(w,s,50);check(count(dst,Item::Diamond)==3,"hopper_unlock");
 auto* hopper=w.chestAt({3,301,3});for(auto& stack:dst->slots)stack={Item::Dirt,64};hopper->slots[0]={Item::IronPickaxe,1,17};ticks(w,s,20);check(hopper->slots[0].durability==17,"full_destination_preserves_source");
 put(w,{8,300,8},Block::Furnace);put(w,{8,301,8},blockState(Block::Hopper,4));put(w,{7,300,8},blockState(Block::Hopper,1));put(w,{8,299,8},blockState(Block::Hopper,3));w.chestAt({8,301,8},true)->slots[0]={Item::IronOre,3};w.chestAt({7,300,8},true)->slots[0]={Item::Coal,2};auto* furnace=w.furnaceAt({8,300,8},true);furnace->output={Item::IronIngot,2};ticks(w,s,40);check(furnace->input.item==Item::IronOre&&furnace->input.count==3&&furnace->fuel.item==Item::Coal&&furnace->fuel.count==2,"furnace_top_input_side_fuel");check(furnace->output.empty()&&count(w.chestAt({8,299,8}),Item::IronIngot)==2,"furnace_bottom_extract_output_only");
 put(w,{12,300,8},blockState(Block::Hopper,1));put(w,{13,300,8},blockState(Block::Hopper,3));w.chestAt({12,300,8},true)->slots[0]={Item::FilledMap,1,0,977};ticks(w,s,500);int total=0;for(auto p:{glm::ivec3(12,300,8),glm::ivec3(13,300,8)})for(auto stack:w.chestAt(p)->slots)if(stack.item==Item::FilledMap){total+=stack.count;check(stack.identity==977,"cycle_identity");}check(total==1,"hopper_cycle_no_duplication");
}
void AutomationTest::machinery(){
 World w(12,22);empty(w,3);SurvivalWorld s(12);check(FarmingSystem::harvest(Block::PistonHead,{0,300,0},12).empty(),"internal_piston_head_has_no_invalid_drop_stack");auto piston=glm::ivec3(5,300,5);put(w,piston,blockState(Block::StickyPiston,1));put(w,{6,300,5},blockState(Block::BirchStairs,4|2));put(w,{5,300,6},blockState(Block::Lever,8));ticks(w,s,3);check(blockType(get(w,{6,300,5}))==Block::PistonHead&&get(w,{7,300,5})==blockState(Block::BirchStairs,4|2),"piston_push_complete_state");w.automation.interact(w,{5,300,6});ticks(w,s,4);check(get(w,{6,300,5})==blockState(Block::BirchStairs,4|2)&&get(w,{7,300,5})==Block::Air,"sticky_pull_state");
 put(w,{6,300,5},Block::Chest);w.chestAt({6,300,5},true)->slots[0]={Item::Diamond,17};w.automation.interact(w,{5,300,6});ticks(w,s,4);check(!(blockProperties(get(w,piston))&8)&&count(w.chestAt({6,300,5}),Item::Diamond)==17,"container_immovable_ownership");
 put(w,{6,300,5},Block::Air);put(w,{5,300,6},Block::Air);for(int i=1;i<=13;++i)put(w,piston+glm::ivec3(i,0,0),Block::Stone);put(w,{5,300,6},blockState(Block::Lever,8));ticks(w,s,4);check(!(blockProperties(get(w,piston))&8),"thirteen_blocks_rejected_atomically");put(w,{18,300,5},Block::Air);ticks(w,s,8);check(blockProperties(get(w,piston))&8,"obstruction_removal_retries_without_local_retrigger");w.automation.interact(w,{5,300,6});ticks(w,s,2);w.automation.interact(w,{5,300,6});ticks(w,s,4);check(blockProperties(get(w,piston))&8,"twelve_blocks_allowed");put(w,piston,Block::Air);check(get(w,{6,300,5})==Block::Air,"broken_piston_removes_head");
 put(w,{5,300,12},blockState(Block::Piston,1));put(w,{5,300,13},blockState(Block::Lever,8));Player player({6.5f,300,12.5f});ticks(w,s,3,&player);check(!(blockProperties(get(w,{5,300,12}))&8),"piston_player_obstruction");
 World negative(41,22);empty(negative,2);SurvivalWorld ns(41);put(negative,{-10,-30,-10},blockState(Block::StickyPiston,4));put(negative,{-10,-29,-10},Block::WoodenSlabTop);put(negative,{-9,-30,-10},blockState(Block::Lever,8));ticks(negative,ns,4);check(get(negative,{-10,-28,-10})==Block::WoodenSlabTop,"negative_y_vertical_legacy_slab_push");negative.automation.interact(negative,{-9,-30,-10});ticks(negative,ns,4);check(get(negative,{-10,-29,-10})==Block::WoodenSlabTop,"negative_y_vertical_sticky_pull");
 put(w,{15,300,20},blockState(Block::Piston,1));put(w,{16,300,20},Block::Stone);w.automation.suspendChunk(1,1);auto unloaded=std::move(w.chunks_.at(w.chunkKey(1,1)));w.chunks_.erase(w.chunkKey(1,1));put(w,{14,300,20},blockState(Block::Lever,8));ticks(w,s,8);check(!(blockProperties(get(w,{15,300,20}))&8),"piston_unloaded_boundary_rejected");w.chunks_.emplace(w.chunkKey(1,1),std::move(unloaded));w.automation.activateChunk(w,1,1);ticks(w,s,8);check(blockType(get(w,{16,300,20}))==Block::PistonHead&&get(w,{17,300,20})==Block::Stone,"piston_loaded_boundary_resumes_atomically");
 put(negative,{-4,-30,-10},blockState(Block::StickyPiston,1|8));put(negative,{-3,-30,-10},blockState(Block::PistonHead,0));put(negative,{-2,-30,-10},Block::Stone);ticks(negative,ns,3);check(!(blockProperties(get(negative,{-4,-30,-10}))&8)&&get(negative,{-3,-30,-10})==Block::PistonHead&&get(negative,{-2,-30,-10})==Block::Stone,"mismatched_head_never_pulled_or_destroyed");
 put(negative,{-4,-30,-5},blockState(Block::Piston,1));put(negative,{-3,-30,-5},Block::Grass);put(negative,{-3,-29,-5},Block::RedFlower);put(negative,{-4,-30,-4},blockState(Block::Lever,8));ticks(negative,ns,3);check(!(blockProperties(get(negative,{-4,-30,-5}))&8)&&get(negative,{-3,-30,-5})==Block::Grass&&get(negative,{-3,-29,-5})==Block::RedFlower,"plant_support_is_immovable_without_detaching_flowers");
 // Working farm: sensor -> harvester -> hopper -> barrel. Replant pays real seeds.
 World farm(19,22);empty(farm,2);SurvivalWorld fs(19);put(farm,{5,300,8},Block::AutoHarvester);put(farm,{5,299,8},blockState(Block::Hopper,1));put(farm,{6,299,8},Block::Barrel);put(farm,{5,301,8},Block::CropSensor);
 for(int x=4;x<=6;++x)for(int z=5;z<=7;++z){put(farm,{x,299,z},Block::WetFarmland);put(farm,{x,300,z},Block::Crop3);}ticks(farm,fs,120);
 auto* store=farm.chestAt({6,299,8});auto* buffer=farm.chestAt({5,299,8});check(count(store,Item::Wheat)+count(buffer,Item::Wheat)>=9,"automated_farm_deposits_real_drops");for(int x=4;x<=6;++x)for(int z=5;z<=7;++z)check(get(farm,{x,300,z})==Block::Crop0,"farm_replants_each_paid_seed");const int wheat=count(store,Item::Wheat)+count(buffer,Item::Wheat);ticks(farm,fs,200);check(count(store,Item::Wheat)+count(buffer,Item::Wheat)==wheat,"immature_farm_no_free_production");
 put(farm,{5,299,7},Block::SugarCane);put(farm,{5,300,7},Block::SugarCane);put(farm,{5,301,8},blockState(Block::Lever,8));ticks(farm,fs,12);check(get(farm,{5,300,7})==Block::Air&&get(farm,{5,299,7})==Block::SugarCane,"sugar_cane_preserves_base");
 for(auto block:{Block::Carrot3,Block::Potato3,Block::Beetroot3,Block::Pumpkin,Block::Melon,Block::BerryBush3}){put(farm,{5,300,7},block);put(farm,{5,299,7},Block::WetFarmland);put(farm,{5,301,8},blockState(Block::Lever,8));ticks(farm,fs,12);check(get(farm,{5,300,7})!=block,"supported_crop_harvest_"+std::to_string(int(block)));}
}
void AutomationTest::persistence(){
 World w(21,22);empty(w,1);SurvivalWorld s(21);put(w,{2,300,2},Block::Repeater);put(w,{2,300,3},blockState(Block::Lever,8));put(w,{5,300,5},blockState(Block::Hopper,4));w.chestAt({5,300,5},true)->slots[0]={Item::FilledMap,1,0,1234};ticks(w,s,1);put(w,{10,300,10},Block::Observer);put(w,{10,300,11},Block::Dirt);put(w,{10,300,11},Block::Air);glm::vec3 pos{1,300,1};check(w.saveWorld("logic.vxw",pos),"world_with_automation_writes");World restored(21,22);check(restored.loadWorld("logic.vxw",pos),"automation_transactional_load");check(restored.chestAt({5,300,5})->slots[0].identity==1234,"loaded_hopper_owns_map");
 std::ifstream file("logic.vxw",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(file)),{});auto tag=bytes.rfind("AUT1");check(tag!=std::string::npos,"automation_extension_present");std::ofstream("legacy.vxw",std::ios::binary).write(bytes.data(),tag);World legacy(21,22);check(legacy.loadWorld("legacy.vxw",pos),"pre_automation_save_loads");
 std::ofstream("truncated.vxw",std::ios::binary).write(bytes.data(),bytes.size()-1);auto before=pos;check(!restored.loadWorld("truncated.vxw",pos)&&pos==before&&restored.chestAt({5,300,5})->slots[0].identity==1234,"corrupt_extension_preserves_active_state");
 empty(restored,1);for(const auto& chunk:restored.edits_)for(auto edit:chunk.second){int cx=int(std::int64_t(chunk.first)>>32),cz=int(std::uint32_t(chunk.first));auto* c=restored.findChunk(cx,cz);if(c)c->setLocal(int(edit.first%16),-64+int(edit.first/256),int(edit.first/16%16),edit.second);}for(int x=-1;x<=1;++x)for(int z=-1;z<=1;++z)restored.automation.activateChunk(restored,x,z);SurvivalWorld rs(21);ticks(restored,rs,1);check(automationSignal(get(restored,{10,300,10}))==15,"saved_pending_observer_pulse_is_delivered");ticks(restored,rs,7);check(automationSignal(get(restored,{2,300,2}))==15,"saved_delayed_signal_resumes");
 auto malformed=bytes;std::uint32_t invalid=0xffffffff;std::memcpy(malformed.data()+tag+8+12,&invalid,4);std::ofstream("bad-node.vxw",std::ios::binary).write(malformed.data(),malformed.size());check(!restored.loadWorld("bad-node.vxw",pos),"invalid_node_type_rejected");
 SaveFile::Checkpoint checkpoint("save.checkpoint");check(checkpoint.ready(),"checkpoint_ready");check(w.saveWorld("voxel_world.vxw",pos)&&checkpoint.commit(),"checkpoint_commits_automation_and_items");check(SaveFile::verifyCheckpoint("."),"checkpoint_checksums_automation");
}

void AutomationTest::production(Game& g){
 auto selected=WorldLibrary::create("Isolated automation checkpoint",188,GameMode::Creative,true);g.playSavedWorld(selected);check(bool(g.world_),"production_world");auto& w=*g.world_;w.prepareSpawnTerrain({8,300,8});w.fillBlocks({0,299,0},{15,299,15},Block::Stone);g.player_->teleport({8.5f,301,8.5f});g.inventory_->clear();
 auto click=[&](glm::ivec3 p){g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.interaction_.hasBlockTarget=true;g.interaction_.mobTarget={};g.interaction_.blockTarget={p,p+glm::ivec3(0,1,0),{0,1,0},3};g.handleUseAction();g.input_.mouseButtons_.fill(false);};
 w.setBlock(5,300,5,Block::Lever);click({5,300,5});check(automationSignal(get(w,{5,300,5}))==15,"actual_rmb_lever");w.setBlock(6,300,5,Block::Repeater);click({6,300,5});check(((blockProperties(get(w,{6,300,5}))>>4)&3)==1,"actual_rmb_repeater_delay");w.setBlock(7,300,5,Block::OakButton);click({7,300,5});check(automationSignal(get(w,{7,300,5}))==15,"actual_rmb_existing_button");
 w.setBlock(5,300,7,blockState(Block::Hopper,4));w.chestAt({5,300,7},true)->slots[0]={Item::FilledMap,1,0,77};click({5,300,7});check(g.ui_.state()==GameState::Chest,"actual_rmb_hopper_existing_container_ui");g.ui_.resumeGame();
 check(g.saveAll(true),"background_checkpoint_queued");w.setBlock(5,300,5,Block::Lever);w.chestAt({5,300,7})->slots[1]={Item::IronSword,1,33};check(g.saveAll(true),"coalesced_changed_automation_checkpoint");g.pollSave(true);g.pollSave(true);check(SaveFile::verifyCheckpoint(selected.directory.string()),"complete_automation_checkpoint_verified");
 g.returnToMainMenu();g.playSavedWorld(selected);check(g.world_&&get(*g.world_,{5,300,5})==Block::Lever,"automation_lever_persistence");auto c=g.world_->chestAt({5,300,7});check(c&&c->slots[0].identity==77&&c->slots[1].durability==33,"hopper_checkpoint_metadata_coherent");g.returnToMainMenu();
}

// Deliberately accumulate failures so one candidate run reproduces independent
// defects. The fixture runs only beneath StabilityDriver's fresh temp profile.
void AutomationTest::stabilization(Game& g,const std::filesystem::path& out){
 int failures=0;auto expect=[&](bool ok,const std::string& name){std::cout<<"stabilization "<<name<<" passed="<<ok<<std::endl;if(!ok)++failures;};
 for(auto door:{Block::OakFenceGate,Block::OakTrapdoor,Block::OakDoor,Block::DoorClosedNorthLower}){
  auto wWorldFixture=std::make_unique<World>(1,22);auto& w=*wWorldFixture;empty(w,1);SurvivalWorld s(1);auto p=glm::ivec3(5,300,5);put(w,p,door);bool legacy=isDoor(door),pair=legacy||buildingIsDoor(door);
  if(pair)put(w,p+glm::ivec3(0,1,0),legacy?Block::DoorClosedNorthUpper:blockState(door,16));
  auto source=p+glm::ivec3(-1,0,0);put(w,source,blockState(Block::Lever,8));ticks(w,s,5);
  auto closed=[&](glm::ivec3 q){auto b=get(w,q);return legacy?doorBlock(doorFacing(b),isDoorUpper(b),doorHingeRight(b),false):blockState(b,blockProperties(b)&~8);};
  auto open=[&](glm::ivec3 q){auto b=get(w,q);return legacy?isDoorOpen(b):bool(blockProperties(b)&8);};
  put(w,p,closed(p));if(pair)put(w,p+glm::ivec3(0,1,0),closed(p+glm::ivec3(0,1,0)));ticks(w,s,3);
  w.automation.interact(w,source);ticks(w,s,4);w.automation.interact(w,source);ticks(w,s,4);
  expect(open(p)&&(!pair||open(p+glm::ivec3(0,1,0))),"manual_override_rearm_"+std::to_string(int(door)));
  put(w,p,closed(p));if(pair)put(w,p+glm::ivec3(0,1,0),closed(p+glm::ivec3(0,1,0)));auto other=p+glm::ivec3(1,0,0);put(w,other,blockState(Block::Lever,8));ticks(w,s,3);w.automation.interact(w,source);ticks(w,s,3);expect(!open(p),"multiple_sources_preserve_manual_override");w.automation.interact(w,other);ticks(w,s,4);
  std::ostringstream saved(std::ios::binary);expect(w.automation.write(saved),"closed_door_edge_serializes");Automation restored;std::istringstream bytes(saved.str(),std::ios::binary);expect(restored.read(bytes,-64,320),"closed_door_edge_loads");w.automation=std::move(restored);w.automation.activateChunk(w,0,0);ticks(w,s,2);w.automation.interact(w,source);ticks(w,s,4);expect(open(p)&&(!pair||open(p+glm::ivec3(0,1,0))),"door_rearms_after_saved_no_op_falling_edge");
  for(int cycle=0;cycle<5;++cycle){w.automation.interact(w,source);ticks(w,s,2);w.automation.interact(w,source);ticks(w,s,2);}expect(open(p)&&(!pair||open(p+glm::ivec3(0,1,0))),"rapid_door_edges_converge");

 }
 for(int upper:{0,1}){auto wWorldFixture=std::make_unique<World>(2,22);auto& w=*wWorldFixture;empty(w,1);SurvivalWorld s(2);put(w,{5,300,5},Block::DoorClosedNorthLower);put(w,{5,301,5},Block::DoorClosedNorthUpper);put(w,{4,300+upper,5},blockState(Block::Lever,8));ticks(w,s,6);expect(isDoorOpen(get(w,{5,300,5}))&&isDoorOpen(get(w,{5,301,5})),"legacy_both_halves_power_"+std::to_string(upper));}
 for(int height:{2,3}){auto wWorldFixture=std::make_unique<World>(3,22);auto& w=*wWorldFixture;empty(w,1);SurvivalWorld s(3);put(w,{5,300,5},Block::AutoHarvester);put(w,{5,299,5},Block::Chest);put(w,{4,300,5},blockState(Block::Lever,8));put(w,{5,298,4},Block::Sand);put(w,{4,298,4},Block::Water);for(int y=0;y<height;++y)put(w,{5,299+y,4},Block::SugarCane);ticks(w,s,60);FarmingSystem farm;farm.update(5,w,glm::vec3(5,300,5));expect(count(w.chestAt({5,299,5}),Item::SugarCane)==height-1&&get(w,{5,299,4})==Block::SugarCane,"cane_collects_all_"+std::to_string(height));}

 {auto farmWorldFixture=std::make_unique<World>(66,22);auto& farm=*farmWorldFixture;empty(farm,1);SurvivalWorld mobs(66);put(farm,{5,298,4},Block::Sand);put(farm,{4,298,4},Block::Water);for(int y=299;y<=301;++y)put(farm,{5,y,4},Block::SugarCane);
  put(farm,{5,300,5},Block::AutoHarvester);put(farm,{5,299,5},Block::Chest);put(farm,{4,300,5},blockState(Block::Lever,8));put(farm,{5,301,3},blockState(Block::AutoHarvester,2));put(farm,{5,300,3},Block::Chest);put(farm,{6,301,3},blockState(Block::Lever,8));
  auto* a=farm.chestAt({5,299,5},true);auto* b=farm.chestAt({5,300,3},true);for(auto* chest:{a,b})for(auto& stack:chest->slots)stack={Item::Dirt,64};ticks(farm,mobs,30);
  expect(get(farm,{5,300,4})==Block::SugarCane&&get(farm,{5,301,4})==Block::SugarCane,"full_cane_outputs_preserve_every_segment");a->slots[0]={};b->slots[0]={};ticks(farm,mobs,30);
  expect(count(a,Item::SugarCane)+count(b,Item::SugarCane)==2&&get(farm,{5,299,4})==Block::SugarCane&&get(farm,{5,300,4})==Block::Air,"two_harvesters_different_heights_conserve_cane");
  glm::vec3 location;expect(farm.saveWorld("cane.vxw",{5,300,5}),"cane_world_save");auto restoredWorldFixture=std::make_unique<World>(66,22);auto& restored=*restoredWorldFixture;expect(restored.loadWorld("cane.vxw",location)&&count(restored.chestAt({5,299,5}),Item::SugarCane)+count(restored.chestAt({5,300,3}),Item::SugarCane)==2,"cane_output_save_reload");
 }
 {auto wWorldFixture=std::make_unique<World>(4,22);auto& w=*wWorldFixture;empty(w,1);SurvivalWorld s(4);put(w,{5,300,5},blockState(Block::Repeater,48));ticks(w,s,2);put(w,{5,300,6},blockState(Block::Lever,8));ticks(w,s,1);const auto remaining=w.automation.nodes_.at(glm::ivec3(5,300,5)).due-w.automation.stats_.ticks;
  w.setSimulationDistance(2);w.streamCenterValid_=true;w.streamCenter_={100,100};ticks(w,s,40);w.streamCenterValid_=false;ticks(w,s,1);auto n=w.automation.nodes_.at(glm::ivec3(5,300,5));expect(n.remaining==remaining||(n.due>=w.automation.stats_.ticks+remaining-1),"timer_keeps_remaining_outside_simulation");}
 {auto wWorldFixture=std::make_unique<World>(5,22);auto& w=*wWorldFixture;empty(w,1);SurvivalWorld s(5);SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType::Chicken;a.position={0,300,0};a.health=4;s.animals_.push_back(a);a.position={2,300,0};a.health=0;a.deathTimer=.5f;a.loveTimer=20;s.animals_.push_back(a);s.feedAnimal({0,300.5f,3},{0,0,-1},Item::Seeds,&w);expect(s.animals_.size()==2,"dying_partner_rejected");}
 // Capacity is filled with inert saved nodes, avoiding generation of 100k chunks.
 for(int capacity:{99998,99999,100000}){auto wWorldFixture=std::make_unique<World>(6,22);auto& w=*wWorldFixture;empty(w,1);for(int i=0;i<capacity;++i){LogicPosition p{glm::ivec3(1000+i,0,1000)};w.automation.nodes_[p]={};w.automation.nodes_[p].type=Block::Lever;}
  auto a=glm::ivec3(5,300,5),b=a+glm::ivec3(0,1,0);bool success=w.automationBatch({{a,Block::Air},{b,Block::Air}},{{a,Block::OakDoor},{b,blockState(Block::OakDoor,16)}});
  expect(success==(capacity==99998)&&((get(w,a)==Block::Air)==(capacity!=99998))&&((get(w,b)==Block::Air)==(capacity!=99998)),"atomic_capacity_"+std::to_string(capacity));}

 for(auto device:{blockState(Block::Repeater,48),Block::OakButton,Block::Observer,Block::DaylightSensor}){
  auto timingWorldFixture=std::make_unique<World>(19,22);auto& timing=*timingWorldFixture;empty(timing,1);SurvivalWorld mobs(19);const glm::ivec3 p(15,300,5);put(timing,p,device);ticks(timing,mobs,2);
  timing.automation.nodes_.at(p).target=15;timing.automation.schedule(p,4);
  for(int cycle=0;cycle<3;++cycle){
   const auto before=timing.automation.remainingTicks(p,timing.automation.nodes_.at(p));timing.automation.suspendChunk(1,0);auto unload=std::move(timing.chunks_.at(timing.chunkKey(1,0)));timing.chunks_.erase(timing.chunkKey(1,0));ticks(timing,mobs,35);
   auto& node=timing.automation.nodes_.at(p);expect(node.due==0&&node.remaining==before,"boundary_delay_frozen_"+std::to_string(int(blockType(device)))+"_"+std::to_string(cycle));
   timing.automation.activateChunk(timing,0,0);ticks(timing,mobs,15);expect(node.due==0&&node.remaining==before,"incomplete_boundary_not_reactivated_"+std::to_string(int(blockType(device))));
   std::ostringstream saved(std::ios::binary);expect(timing.automation.write(saved),"paused_timer_serializes");Automation restored;std::istringstream input(saved.str(),std::ios::binary);expect(restored.read(input,-64,320)&&restored.nodes_.at(p).remaining==before,"paused_timer_roundtrip");
   timing.chunks_.emplace(timing.chunkKey(1,0),std::move(unload));timing.automation.activateChunk(timing,1,0);expect(timing.automation.remainingTicks(p,node)==before,"resume_keeps_delay");
  }
 }
 {auto rangeWorldFixture=std::make_unique<World>(1,22);auto& range=*rangeWorldFixture;range.streamCenterValid_=true;range.streamCenter_={100,100};range.setSimulationDistance(2);expect(!range.simulationActiveAt(5,5),"distance_setting_never_reinitializes_simulation_center");}
 {auto staleWorldFixture=std::make_unique<World>(1,22);auto& stale=*staleWorldFixture;empty(stale,1);SurvivalWorld m(1);for(int i=0;i<10000;++i)stale.automation.changes_.push_back({glm::ivec3(i,300,0),Block::Lever,blockState(Block::Lever,8),0});ticks(stale,m,1);expect(stale.automation.changes_.size()>=10000-512,"stale_commits_also_consume_bounded_queue_work");}
 // Preparation is bounded even with 100k spatially sparse saved registrations.
 {auto manyWorldFixture=std::make_unique<World>(20,22);auto& many=*manyWorldFixture;empty(many,1);SurvivalWorld mobs(20);for(int i=0;i<100000;++i){auto key=LogicPosition(glm::ivec3(i*16,0,1000));many.automation.nodes_[key].type=Block::Lever;many.automation.chunks_[{i,62}].insert(key);}
  for(int i=0;i<220;++i){ticks(many,mobs,1);const auto& d=many.automation.diagnostics();expect(d.reconciled<=512&&d.dueProcessed<=2048&&d.processed<=2048&&d.changes<=512&&d.tickMilliseconds>=d.preparationMilliseconds&&d.tickMilliseconds>=d.evaluationMilliseconds,"all_tick_phases_accounted_and_bounded");}
  expect(!many.automation.reconcilePending_,"sparse_registry_eventual_reconciliation");}
 std::cout<<"fixture creating production world / sizeof World="<<sizeof(World)<<std::endl;auto selected=WorldLibrary::create("Prerelease stabilization isolated",233,GameMode::Survival,true);std::cout<<"fixture loading world"<<std::endl;g.playSavedWorld(selected);expect(bool(g.world_),"production_world");auto& w=*g.world_;auto& s=*g.survival_;auto& inv=*g.inventory_;w.prepareSpawnTerrain({8,300,8});w.fillBlocks({0,299,0},{15,299,15},Block::Stone);g.player_->teleport({8.5f,300,8.5f});

 for(auto item:{Item::OakDoor,Item::WoodenDoor})for(int capacity:{99998,99999,100000}){
  auto original=std::move(w.automation);w.automation=Automation{};for(int i=0;i<capacity;++i){auto key=LogicPosition(glm::ivec3(1000+i,0,1000));w.automation.nodes_[key].type=Block::Lever;}
  inv.clear();inv.add(item);g.input_.keys_.fill(false);g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.interaction_.hasBlockTarget=true;g.interaction_.mobTarget={};g.interaction_.blockTarget={{4,299,11},{4,300,11},{0,1,0},3};
  const auto preview=g.placementPlan(item);g.handleUseAction();const bool accepted=capacity==99998;
  expect(preview.valid==accepted&&inv.count(item)==(accepted?0:1)&&(get(w,{4,300,11})!=Block::Air)==accepted&&(get(w,{4,301,11})!=Block::Air)==accepted,"RMB_door_capacity_"+std::to_string(int(item))+"_"+std::to_string(capacity));
  w.setBlockInternal(4,301,11,Block::Air,true,false);w.setBlockInternal(4,300,11,Block::Air,true,false);w.automation=std::move(original);g.input_.mouseButtons_.fill(false);
 }
 s.spawnDrop({8,300,8},Item::Stone);s.drops_.resize(SaveValidation::MaxDrops,s.drops_.front());
 inv.clear();inv.add(Item::FilledMap,1,0,613);g.input_.keys_.fill(false);g.input_.previousKeys_.fill(false);g.input_.keys_[GLFW_KEY_Q]=true;g.handleDropAction();expect(inv.count(Item::FilledMap)==1||!w.buildingDrops_.empty(),"gameplay_Q_keeps_owner_at_drop_limit");g.input_.keys_.fill(false);
 put(w,{3,300,3},Block::Composter4);FarmingSystem farm;farm.interact(w,inv,s,{3,300,3},{3,301,3},{0,1,0},Item::None,false,false);expect(get(w,{3,300,3})==Block::Composter4||!w.buildingDrops_.empty(),"composter_keeps_production_at_drop_limit");
 put(w,{4,300,3},Block::BerryBush3);farm.interact(w,inv,s,{4,300,3},{4,301,3},{0,1,0},Item::None,false,false);expect(get(w,{4,300,3})==Block::BerryBush3||!w.buildingDrops_.empty(),"berries_keep_production_at_drop_limit");
 {SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType::Sheep;a.position={5,300,5};a.health=8;s.animals_.push_back(a);inv.clear();inv.add(Item::Shears,1,20);s.tendAnimal(int(s.animals_.size()-1),inv,false,false);expect(!s.animals_.back().sheared&&inv.selectedStack().durability==20,"shear_keeps_wool_and_tool_at_drop_limit");}
 g.ui_.openInventory();g.ui_.recipeBook_.searchFocused=true;g.ui_.recipeBook_.search.clear();g.input_.typedCharacters_="stone bricks";int W,H;glfwGetFramebufferSize(g.window_,&W,&H);g.ui_.updateInventoryInteraction(g.input_,W,H,inv,s,w,*g.player_,false);expect(g.ui_.recipeBook_.search=="stone bricks","recipe_keeps_all_character_events");g.ui_.closeGameplayInterface(inv);

 // Drive the GUI through actual framebuffer hit tests and existing input routes.
 glfwShowWindow(g.window_);glfwFocusWindow(g.window_);glfwPollEvents();g.input_.setCursorCaptured(true);g.settings_.uiScale=2;
 glfwGetFramebufferSize(g.window_,&W,&H);int ww,wh;glfwGetWindowSize(g.window_,&ww,&wh);
 const float scale=UiLayout::interfaceScale(W,H,2);const int lw=int(W/scale),lh=int(H/scale);
 auto cursor=[&](double x,double y){glfwSetCursorPos(g.window_,x*scale*ww/W,y*scale*wh/H);};
 auto resetInput=[&]{g.input_.keys_.fill(false);g.input_.previousKeys_.fill(false);g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.typedCharacters_.clear();g.ui_.inventoryLeftWasDown_=false;g.ui_.dragHit_={};};
 auto gui=[&](bool creative){g.ui_.updateInventoryInteraction(g.input_,W,H,inv,s,w,*g.player_,creative);};
 for(bool creative:{false,true})for(bool entire:{false,true}){
  resetInput();g.ui_.openInventory();g.ui_.recipeBook_.searchFocused=false;g.ui_.creativeSearchFocused_=false;inv.clear();inv.slots_[0]={Item::Stone,7};auto r=UiLayout::playerSlot(creative?UiMode::Creative:UiMode::Inventory,0,lw,lh);cursor(r.x+10,r.y+10);g.input_.keys_[GLFW_KEY_Q]=true;g.input_.keys_[GLFW_KEY_LEFT_CONTROL]=entire;gui(creative);
  expect(inv.slot(0).count==7&&s.drops_.size()==SaveValidation::MaxDrops,"gui_Q_refusal_"+std::to_string(creative)+std::to_string(entire));
  s.drops_.pop_back();g.input_.previousKeys_.fill(false);gui(creative);expect(inv.slot(0).count==(entire?0:6)&&s.drops_.size()==SaveValidation::MaxDrops&&s.drops_.back().stack.count==(entire?7:1),"gui_Q_one_acceptance_"+std::to_string(creative)+std::to_string(entire));
 }
 for(bool creative:{false,true})for(bool right:{false,true}){
  resetInput();g.ui_.openInventory();inv.clear();inv.cursor_={Item::IronSword,1,17};cursor(2,2);g.input_.mouseButtons_[right?GLFW_MOUSE_BUTTON_RIGHT:GLFW_MOUSE_BUTTON_LEFT]=true;gui(creative);
  expect(inv.cursorStack().item==Item::IronSword&&inv.cursorStack().durability==17,"outside_click_keeps_gear_"+std::to_string(creative)+std::to_string(right));
 }
 for(bool creative:{false,true}){resetInput();g.ui_.openInventory();inv.clear();inv.cursor_={Item::FilledMap,1,0,713};cursor(2,2);g.ui_.inventoryLeftWasDown_=true;gui(creative);expect(inv.cursorStack().identity==713,"drag_release_keeps_identity_"+std::to_string(creative));}
 resetInput();g.ui_.closeGameplayInterface(inv);glfwHideWindow(g.window_);
 // Both output limits exhausted: a whole batch must reject before ownership moves.
 w.buildingDrops_.assign(100000,{{8,300,8},{Item::Stone,1}});const auto oldQueue=w.buildingDrops_.size();
 expect(!w.retainBuildingDrops({8,300,8},{{Item::Diamond,1},{Item::FilledMap,1,0,99}})&&w.buildingDrops_.size()==oldQueue,"recovery_queue_batch_rejects_without_overflow");
 put(w,{6,300,3},Block::Barrel);auto* fullBarrel=w.chestAt({6,300,3},true);fullBarrel->slots[0]={Item::FilledMap,1,0,918};
 expect(!w.setBlockInternal(6,300,3,Block::Air,true,false)&&w.chestAt({6,300,3})->slots[0].identity==918,"direct_storage_removal_rejects_when_recovery_full");
 g.player_->teleport({6.5f,300,5.5f});g.interaction_.hasBlockTarget=true;g.interaction_.mobTarget={};g.interaction_.blockTarget={{6,300,3},{6,300,4},{0,0,1},2};g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_LEFT]=true;g.updateMining(100);
 expect(get(w,{6,300,3})==Block::Barrel&&w.chestAt({6,300,3})->slots[0].identity==918,"mining_refusal_retains_container_and_item");resetInput();
 w.buildingDrops_.clear();
 for(auto block:{Block::Barrel,Block::Hopper,Block::ItemFrame}){
  put(w,{6,300,3},block);w.chestAt({6,300,3},true)->slots[0]={Item::FilledMap,1,0,918};w.chestAt({6,300,3},true)->slots[1]={Item::DiamondSword,1,23};
  const auto queued=w.buildingDrops_.size();s.detonate({6.5f,300.5f,3.5f},w,*g.player_);int maps=0,gear=0;for(auto d:w.buildingDrops_){if(d.second.identity==918)maps+=d.second.count;if(d.second.item==Item::DiamondSword&&d.second.durability==23)gear+=d.second.count;}
  expect(get(w,{6,300,3})==Block::Air&&maps==1&&gear==1&&w.buildingDrops_.size()>queued,"explosion_retains_all_contents_"+std::to_string(int(block)));
  w.buildingDrops_.clear();
 }
 // A refused explosion leaves original storage in place, even at both limits.
 put(w,{6,300,3},Block::Hopper);w.chestAt({6,300,3},true)->slots[0]={Item::FilledMap,1,0,921};w.buildingDrops_.assign(100000,{{8,300,8},{Item::Stone,1}});s.detonate({6.5f,300.5f,3.5f},w,*g.player_);
 expect(blockType(get(w,{6,300,3}))==Block::Hopper&&w.chestAt({6,300,3})->slots[0].identity==921,"explosion_refusal_retains_source_at_both_limits");w.buildingDrops_.clear();
 // Boat break must keep the entity if no drop can be admitted, and recover once.
 {auto boatsWorldFixture=std::make_unique<World>(15,22);auto& boats=*boatsWorldFixture;empty(boats,1);for(int z=2;z<9;++z)for(int x=2;x<9;++x)put(boats,{x,299,z},Block::Water);Transportation travel({5,300,5});expect(travel.placeBoat(boats,Item::OakBoat,{5.5f,300,5.5f},0),"boat_drop_fixture");Player p({5.5f,300,8.5f});p.pitch_=-25;p.yaw_=-90;
  expect(!travel.breakBoat(p,inv,s,boats,5)&&travel.boats().size()==1,"boat_break_refusal_keeps_boat");s.drops_.pop_back();expect(travel.breakBoat(p,inv,s,boats,5)&&travel.boats().empty()&&s.drops_.back().stack.item==Item::OakBoat,"boat_break_exactly_once");}
 // Pending ownership survives a complete checkpoint, interruption and reload.
 w.retainBuildingDrops({8,300,8},{{Item::FilledMap,1,0,719},{Item::DiamondSword,1,29},{Item::Stone,37}});
 expect(g.saveAll(),"recovery_checkpoint_complete");expect(SaveFile::verifyCheckpoint(selected.directory.string()),"recovery_checkpoint_verified");
 const auto savedWorldPath=selected.directory/"voxel_world.vxw";glm::vec3 restoredPosition;auto restoreWorldFixture=std::make_unique<World>(233,22);auto& restore=*restoreWorldFixture;expect(restore.loadWorld(savedWorldPath.string(),restoredPosition)&&restore.buildingDrops_.size()==3&&restore.buildingDrops_[0].second.identity==719&&restore.buildingDrops_[1].second.durability==29,"deferred_batch_roundtrip_metadata");
 SurvivalWorld recovered(233);restore.flushBuildingDrops(recovered);const auto delivered=recovered.drops_.size();restore.flushBuildingDrops(recovered);expect(delivered==3&&recovered.drops_.size()==3&&restore.buildingDrops_.empty(),"recovery_delivers_exactly_once");

 expect(g.saveAll(true),"pending_recovery_background_snapshot");s.drops_.resize(s.drops_.size()-3);w.flushBuildingDrops(s);expect(w.buildingDrops_.empty(),"deferred_output_transferred_to_live_owner");expect(g.saveAll(true),"coalesced_live_output_snapshot");g.pollSave(true);g.pollSave(true);expect(SaveFile::verifyCheckpoint(selected.directory.string()),"coalesced_recovery_manifest_verified");
 {auto qWorldFixture=std::make_unique<World>(233,22);auto& q=*qWorldFixture;SurvivalWorld m(233);glm::vec3 location;expect(q.loadWorld(savedWorldPath.string(),location)&&m.load((selected.directory/"voxel_mobs.vxm").string(),233),"coalesced_recovery_world_mobs_load");int maps=0,gear=0;for(auto d:m.drops_){if(d.stack.identity==719)maps+=d.stack.count;if(d.stack.item==Item::DiamondSword&&d.stack.durability==29)gear+=d.stack.count;}expect(q.buildingDrops_.empty()&&maps==1&&gear==1,"coalesced_checkpoint_has_exactly_one_owner");}

 const auto recoveryCopy=std::filesystem::absolute("previous-ownership-copy");expect(SaveFile::copyPrevious(selected.directory.string(),recoveryCopy.string()),"explicit_previous_checkpoint_recovery_copy");
 {auto qWorldFixture=std::make_unique<World>(233,22);auto& q=*qWorldFixture;SurvivalWorld m(233);glm::vec3 location;expect(q.loadWorld((recoveryCopy/"voxel_world.vxw").string(),location)&&m.load((recoveryCopy/"voxel_mobs.vxm").string(),233),"verified_backup_components_load");int maps=0,gear=0;for(auto d:m.drops_){if(d.stack.identity==719)maps+=d.stack.count;if(d.stack.item==Item::DiamondSword&&d.stack.durability==29)gear+=d.stack.count;}for(auto d:q.buildingDrops_){if(d.second.identity==719)maps+=d.second.count;if(d.second.item==Item::DiamondSword&&d.second.durability==29)gear+=d.second.count;}expect(maps==1&&gear==1&&SaveFile::verifyCheckpoint(selected.directory.string()),"backup_ownership_conserved_source_unchanged");
  {SaveFile::Checkpoint interrupted((recoveryCopy/"save.checkpoint").string());expect(interrupted.ready()&&SaveFile::write((recoveryCopy/"world_seed.txt").string(),std::ios::out,[](std::ofstream& f){f<<"incomplete candidate";return bool(f);}),"incomplete_snapshot_staged");}
  expect(SaveFile::verifyCheckpoint(recoveryCopy.string()),"aborted_snapshot_keeps_verified_previous_checkpoint");
 }
 // Fast text input, paste restrictions, focus switches and key repeat.
 resetInput();g.ui_.openInventory();g.ui_.recipeBook_.searchFocused=true;g.ui_.recipeBook_.search.clear();g.input_.typedCharacters_="STONE bricks!! 123";gui(false);expect(g.ui_.recipeBook_.search=="stone bricks ","recipe_validates_order_and_characters");
 g.input_.keys_[GLFW_KEY_BACKSPACE]=true;gui(false);auto first=g.ui_.recipeBook_.search;g.input_.previousKeys_=g.input_.keys_;g.ui_.recipeRepeatAt_=glfwGetTime()-.001;gui(false);expect(g.ui_.recipeBook_.search.size()+1==first.size(),"recipe_backspace_repeat");
 resetInput();glfwSetClipboardString(g.window_,"COBBLESTONE123\nSTONE");g.input_.keys_[GLFW_KEY_LEFT_CONTROL]=true;g.input_.keys_[GLFW_KEY_A]=true;gui(false);g.input_.keys_[GLFW_KEY_A]=false;g.input_.keys_[GLFW_KEY_V]=true;gui(false);expect(g.ui_.recipeBook_.search=="cobblestonestone","recipe_clipboard_sanitized");
 resetInput();g.ui_.recipeBook_.searchFocused=false;g.ui_.closeGameplayInterface(inv);g.ui_.openInventory();g.ui_.creativeSearchFocused_=true;g.input_.typedCharacters_="fence";gui(true);expect(g.ui_.creativeSearch_=="fence"&&g.ui_.recipeBook_.search=="cobblestonestone"&&g.input_.consumeTypedCharacters().empty(),"search_focus_single_consumer");g.ui_.creativeSearchFocused_=false;resetInput();g.ui_.closeGameplayInterface(inv);
 // Precision evidence is reported separately: no partial coordinate migration.
 std::ofstream precision(out/"precision.csv");precision<<"coordinate,dx_after_120_steps\n";
 for(float x:{1000000.f,8388608.f,29999999.f,-1000000.f,-8388608.f,-29999999.f}){Player p({x,300,x});p.setSpectatorMode(true);p.yaw_=0;p.pitch_=0;PlayerInput input;input.enabled=true;input.moveForward=true;for(int i=0;i<120;++i)p.update(1.f/120.f,input,w);precision<<x<<','<<p.position().x-x<<'\n';}
 g.input_.keys_.fill(false);g.input_.previousKeys_.fill(false);g.returnToMainMenu();
 std::cout<<"Stabilization failures="<<failures<<std::endl;if(failures)throw std::runtime_error("Prerelease stabilization failures="+std::to_string(failures));
}

void AutomationTest::benchmark(const std::filesystem::path& out){
 std::ofstream phases(out/"automation-phases.csv");phases<<"components,mode,total_mean_ms,preparation_mean_ms,evaluation_mean_ms,commit_mean_ms,max_reconciled,max_due\n";
 std::ofstream csv(out/"automation-cpu.csv");csv<<"components,mode,mean_ms,p95_ms,p99_ms,peak_dirty,world_bytes,save_ms\n";
 for(int n:{100,1000,10000})for(int mode=0;mode<2;++mode){World w(81,22);empty(w,7);SurvivalWorld s(81);int width=int(std::ceil(std::sqrt(float(n))));std::vector<glm::ivec3> sources;for(int i=0;i<n;++i){glm::ivec3 p(i%width,300,i/width);if(i%64==0){sources.push_back(p);w.setBlockInternal(p.x,p.y,p.z,blockState(Block::Lever,8),true,false);}else w.setBlockInternal(p.x,p.y,p.z,Block::PowerWire,true,false);}ticks(w,s,400);std::vector<double> samples;double total=0,preparation=0,evaluation=0,commit=0;unsigned maxReconciled=0,maxDue=0;std::size_t dirty=0;for(int i=0;i<300;++i){if(mode&&i%20==0)for(auto p:sources)w.automation.interact(w,p);auto t=std::chrono::steady_clock::now();ticks(w,s,1);samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());const auto& diag=w.automation.diagnostics();total+=diag.tickMilliseconds;preparation+=diag.preparationMilliseconds;evaluation+=diag.evaluationMilliseconds;commit+=diag.commitMilliseconds;maxReconciled=std::max(maxReconciled,diag.reconciled);maxDue=std::max(maxDue,diag.dueProcessed);dirty=std::max(dirty,diag.dirty);if(diag.changes>512)throw std::runtime_error("Unbounded network commit");}auto t=std::chrono::steady_clock::now();phases<<n<<','<<(mode?"toggling":"idle")<<','<<total/300<<','<<preparation/300<<','<<evaluation/300<<','<<commit/300<<','<<maxReconciled<<','<<maxDue<<'\n';w.saveWorld("bench.vxw",{1,300,1});double save=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();double mean=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();std::sort(samples.begin(),samples.end());csv<<n<<','<<(mode?"toggling":"idle")<<','<<mean<<','<<samples[285]<<','<<samples[297]<<','<<dirty<<','<<std::filesystem::file_size("bench.vxw")<<','<<save<<'\n';}
}
void AutomationTest::visual(Game& g,const std::filesystem::path& out){
 auto world=WorldLibrary::create("Automation visual fixture",817,GameMode::Creative,true);std::cout<<"visual creating world "<<world.directory<<std::endl;g.playSavedWorld(world);check(bool(g.world_),"visual_world_open");auto& w=*g.world_;for(int x=-16;x<=32;x+=16)for(int z=-16;z<=32;z+=16)w.prepareSpawnTerrain({float(x),300,float(z)});std::cout<<"visual terrain ready"<<std::endl;g.player_->teleport({8.5f,303,17.5f});g.player_->yaw_=-90;g.player_->pitch_=-30;g.player_->setFlying(true);w.fillBlocks({-4,299,-4},{23,299,23},Block::StoneBricks);g.settings_.renderDistance=8;g.settings_.interactionHints=true;g.ui_.resumeGame();g.synchronizeCursorCapture();
 for(int x=2;x<=10;++x)w.setBlock(x,300,5,Block::PowerWire);w.setBlock(1,300,5,blockState(Block::Lever,8));w.setBlock(11,300,5,Block::PowerLamp);w.setBlock(6,300,4,Block::OakFenceGate);w.setBlock(10,300,9,blockState(Block::StickyPiston,1));w.setBlock(11,300,9,Block::StoneBricks);w.setBlock(10,300,10,blockState(Block::Lever,8));
 w.setBlock(3,300,12,Block::AutoHarvester);w.setBlock(3,299,12,blockState(Block::Hopper,1));w.setBlock(4,299,12,Block::Barrel);w.setBlock(2,300,12,blockState(Block::Lever,8));for(int x=2;x<=4;++x)for(int z=9;z<=11;++z){w.setBlock(x,299,z,Block::WetFarmland);w.setBlock(x,300,z,Block::Crop3);}
 std::cout<<"visual devices ready"<<std::endl;for(int i=0;i<120;++i){if(i%20==0)std::cout<<"visual frame "<<i<<std::endl;w.automation.update(.1f,w,*g.survival_,g.player_.get(),1);if(i==0)std::cout<<"visual simulated"<<std::endl;w.updateStreaming(g.player_->cameraPosition(),8);if(i==0)std::cout<<"visual streamed"<<std::endl;g.renderFrame(0);if(i==0)std::cout<<"visual rendered"<<std::endl;glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 auto capture=[&](const char* name){g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path last;for(auto file:std::filesystem::directory_iterator("screenshots"))if(last.empty()||file.last_write_time()>std::filesystem::last_write_time(last))last=file.path();std::filesystem::copy_file(last,out/name,std::filesystem::copy_options::overwrite_existing);};capture("working-circuit-farm.bmp");g.player_->teleport({3.5f,302,15.5f});g.player_->pitch_=-35;for(int i=0;i<30;++i){w.updateStreaming(g.player_->cameraPosition(),8);g.renderFrame(0);}capture("automatic-farm.bmp");g.ui_.openChest({4,299,12});g.synchronizeCursorCapture();g.renderFrame(0);capture("harvested-storage.bmp");
}

void setupAutomationTraversal(Game& g,int count){AutomationTest::setupTraversal(g,count);}
void driveAutomationTraversal(Game& g,double seconds){AutomationTest::driveTraversal(g,seconds);}
void AutomationTest::setupTraversal(Game& g,int count){
 auto& w=*g.world_;auto base=glm::ivec3(glm::floor(g.player_->position()))+glm::ivec3(14,0,14);int width=int(std::ceil(std::sqrt(float(count))));
 for(int x=-16;x<=std::max(width,32)+16;x+=16)for(int z=-16;z<=std::max(width,32)+16;z+=16)w.prepareSpawnTerrain(glm::vec3(base+glm::ivec3(x,0,z)));
 traversalSources.clear();traversalPulse=-1;
 for(int i=0;i<count;++i){auto p=base+glm::ivec3(i%width,0,i/width);w.setBlockInternal(p.x,p.y-1,p.z,Block::StoneBricks,true,false);auto b=Block::PowerWire;if(i%64==0){b=blockState(Block::Lever,8);traversalSources.push_back(p);}w.setBlockInternal(p.x,p.y,p.z,b,true,false);}
 // Additional live container, piston and paid-replant farm workloads.
 auto origin=base-glm::ivec3(8,0,0);
 for(int i=0;i<8;++i){auto p=origin+glm::ivec3(0,0,i*2);w.setBlockInternal(p.x,p.y,p.z,blockState(Block::Hopper,1),true,false);w.setBlockInternal(p.x+1,p.y,p.z,blockState(Block::Hopper,3),true,false);auto* storage=w.chestAt(p,true);if(!storage)throw std::runtime_error("Hopper benchmark fixture was not loaded");storage->slots[0]={Item::Stone,32};}
 for(int i=0;i<8;++i){auto piston=origin+glm::ivec3(-4,0,i*2);w.setBlockInternal(piston.x,piston.y,piston.z,blockState(Block::StickyPiston,1),true,false);w.setBlockInternal(piston.x+1,piston.y,piston.z,Block::Stone,true,false);w.setBlockInternal(piston.x+2,piston.y,piston.z,Block::Air,true,false);auto source=piston+glm::ivec3(0,0,1);w.setBlockInternal(source.x,source.y-1,source.z,Block::Stone,true,false);w.setBlockInternal(source.x,source.y,source.z,blockState(Block::Lever,8),true,false);traversalSources.push_back(source);}

 auto farm=origin+glm::ivec3(-8,0,6);w.setBlockInternal(farm.x,farm.y,farm.z,Block::AutoHarvester,true,false);w.setBlockInternal(farm.x,farm.y-1,farm.z,blockState(Block::Hopper,1),true,false);w.setBlockInternal(farm.x+1,farm.y-1,farm.z,Block::Barrel,true,false);w.setBlockInternal(farm.x,farm.y+1,farm.z,Block::CropSensor,true,false);for(int z=1;z<=3;++z)for(int x=-1;x<=1;++x){auto p=farm+glm::ivec3(x,0,-z);w.setBlockInternal(p.x,p.y-1,p.z,Block::WetFarmland,true,false);w.setBlockInternal(p.x,p.y,p.z,Block::Crop3,true,false);}
}
void AutomationTest::driveTraversal(Game& g,double time){int pulse=int(time/2);if(pulse==traversalPulse)return;traversalPulse=pulse;for(auto p:traversalSources)g.world_->automation.interact(*g.world_,p);}

void AutomationTest::systems(const std::filesystem::path& out){
 std::ofstream csv(out/"automation-systems.csv");csv<<"system,components,mean_ms,p95_ms,p99_ms,ram_mb,world_bytes,save_ms\n";
 auto measure=[&](const char* label,int n,World& w,SurvivalWorld& s,const std::function<void(int)>& before){std::vector<double> samples;for(int i=0;i<240;++i){before(i);auto start=std::chrono::steady_clock::now();ticks(w,s,1);samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}auto start=std::chrono::steady_clock::now();check(w.saveWorld("system.vxw",{1,300,1}),"system_save");auto save=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();double mean=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();std::sort(samples.begin(),samples.end());csv<<label<<','<<n<<','<<mean<<','<<samples[228]<<','<<samples[237]<<','<<fixtureWorkingSetMb()<<','<<std::filesystem::file_size("system.vxw")<<','<<save<<'\n';csv.flush();};
 for(int n:{100,1000,10000}){World w(12,22);empty(w,7);SurvivalWorld s(12);int width=int(std::ceil(std::sqrt(float(n))));if(width%2)++width;for(int i=0;i<n;++i){glm::ivec3 p(i%width,300,i/width);w.setBlockInternal(p.x,p.y,p.z,blockState(Block::Hopper,i%2?3:1),true,false);if(i%2==0)w.chestAt(p,true)->slots[0]={Item::Stone,32};}measure("hopper_cycles",n,w,s,[](int){});int total=0;for(const auto& c:w.chests_)for(auto stack:c.second.slots)total+=stack.count;check(total==n/2*32,"large_hopper_network_conserves_items");}
 for(int n:{100,1000}){World w(13,22);empty(w,7);SurvivalWorld s(13);int machines=n/2,width=int(std::ceil(std::sqrt(float(machines))));std::vector<glm::ivec3> sources;for(int i=0;i<machines;++i){glm::ivec3 p(i%width*4,300,i/width*4);w.setBlockInternal(p.x,p.y,p.z,blockState(Block::StickyPiston,1),true,false);w.setBlockInternal(p.x+1,p.y,p.z,Block::Stone,true,false);auto source=p+glm::ivec3(0,0,1);w.setBlockInternal(source.x,source.y,source.z,blockState(Block::Lever,8),true,false);sources.push_back(source);}measure("piston_machines",n,w,s,[&](int i){if(i%20==0)for(auto p:sources)w.automation.interact(w,p);});}
 {World w(14,22);empty(w,5);SurvivalWorld s(14);for(int i=0;i<100;++i){glm::ivec3 p(i%10*6,300,i/10*6);w.setBlockInternal(p.x,p.y,p.z,Block::AutoHarvester,true,false);w.setBlockInternal(p.x,p.y-1,p.z,blockState(Block::Hopper,1),true,false);w.setBlockInternal(p.x+1,p.y-1,p.z,Block::Barrel,true,false);w.setBlockInternal(p.x-1,p.y,p.z,blockState(Block::Lever,8),true,false);for(int z=1;z<=3;++z)for(int x=-1;x<=1;++x){auto q=p+glm::ivec3(x,0,-z);w.setBlockInternal(q.x,q.y-1,q.z,Block::WetFarmland,true,false);w.setBlockInternal(q.x,q.y,q.z,Block::Crop3,true,false);}}measure("100_farms_900_crops",300,w,s,[](int){});int wheat=0;for(auto& c:w.chests_)wheat+=count(&c.second,Item::Wheat);check(wheat>=900,"large_farm_production_in_storage");}
}

void AutomationTest::sparse(const std::filesystem::path& out){
 World w(20,22);empty(w,1);SurvivalWorld s(20);
 for(int i=0;i<100000;++i){auto key=LogicPosition(glm::ivec3(i*16,0,1000));w.automation.nodes_[key].type=Block::Lever;w.automation.chunks_[{i,62}].insert(key);}
 std::ofstream csv(out/"sparse-preparation.csv");csv<<"tick,total_ms\n";
 for(int i=0;i<220;++i){auto start=std::chrono::steady_clock::now();ticks(w,s,1);csv<<i<<','<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';}
}
