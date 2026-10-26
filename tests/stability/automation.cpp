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
 static void benchmark(const std::filesystem::path&);
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

void AutomationTest::benchmark(const std::filesystem::path& out){
 std::ofstream csv(out/"automation-cpu.csv");csv<<"components,mode,mean_ms,p95_ms,p99_ms,peak_dirty,world_bytes,save_ms\n";
 for(int n:{100,1000,10000})for(int mode=0;mode<2;++mode){World w(81,22);empty(w,7);SurvivalWorld s(81);int width=int(std::ceil(std::sqrt(float(n))));std::vector<glm::ivec3> sources;for(int i=0;i<n;++i){glm::ivec3 p(i%width,300,i/width);if(i%64==0){sources.push_back(p);w.setBlockInternal(p.x,p.y,p.z,blockState(Block::Lever,8),true,false);}else w.setBlockInternal(p.x,p.y,p.z,Block::PowerWire,true,false);}ticks(w,s,400);std::vector<double> samples;std::size_t dirty=0;for(int i=0;i<300;++i){if(mode&&i%20==0)for(auto p:sources)w.automation.interact(w,p);auto t=std::chrono::steady_clock::now();ticks(w,s,1);samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());dirty=std::max(dirty,w.automation.diagnostics().dirty);if(w.automation.diagnostics().changes>512)throw std::runtime_error("Unbounded network commit");}auto t=std::chrono::steady_clock::now();w.saveWorld("bench.vxw",{1,300,1});double save=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();double mean=std::accumulate(samples.begin(),samples.end(),0.)/samples.size();std::sort(samples.begin(),samples.end());csv<<n<<','<<(mode?"toggling":"idle")<<','<<mean<<','<<samples[285]<<','<<samples[297]<<','<<dirty<<','<<std::filesystem::file_size("bench.vxw")<<','<<save<<'\n';}
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
