#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "Player.h"
#include "World.h"
#include "Building.h"
#include "Renderer.h"
#include "SaveFile.h"
#include "WorldTime.h"
#include "Farming.h"
#include "UiLayout.h"
#include "AmbientOcclusion.h"
#include "SaveValidation.h"
#include "InventoryTooltip.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <set>
#include <algorithm>
#include <cctype>
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <psapi.h>

class BuildingTest {
 static inline int count=0;
 static void check(bool ok,const std::string& label){++count;std::cout<<"building "<<label<<" passed="<<ok<<std::endl;if(!ok)throw std::runtime_error(label);}
 static std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
 static void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f.write(s.data(),s.size());}
public:static int run(int argc,char** argv);static int visual(Game&,const std::filesystem::path&);static int benchmark(Game&,const std::filesystem::path&);static int creativeUi(Game&,const std::filesystem::path&);
};
int runBuildingTests(int argc,char** argv){return BuildingTest::run(argc,argv);}
int BuildingTest::run(int argc,char** argv){
 Game g;char app[]="building-validation";char* args[]={app};check(g.initialize(1,args),"initialize");
 auto selected=WorldLibrary::create("Isolated building fixture",991,GameMode::Creative,true);g.playSavedWorld(selected);check(bool(g.world_),"world");
 if(argc>1 && std::string(argv[1])=="--creative-ui")return creativeUi(g,argv[2]);
 if(argc>1 && std::string(argv[1])=="--building-visual")return visual(g,argv[2]);
 if(argc>1 && std::string(argv[1])=="--building-benchmark")return benchmark(g,argv[2]);glfwHideWindow(g.window_);
 auto& w=*g.world_;auto& inv=*g.inventory_;auto& mobs=*g.survival_;
 for(int x:{-16,0,16,32,48,64})for(int z:{-16,0,16,32})w.prepareSpawnTerrain({float(x),300,float(z)});
 w.fillBlocks({-16,299,-16},{79,299,47},Block::Stone);w.fillBlocks({-16,300,-16},{79,306,47},Block::Air);
 g.player_->teleport({0.5f,300,8.5f});g.setGameMode(GameMode::Creative);
 for(int page=0;page<int((creativeCatalog().size()+UiLayout::CreativePageSize-1)/UiLayout::CreativePageSize);++page)for(int index=page*UiLayout::CreativePageSize;index<std::min(int(creativeCatalog().size()),(page+1)*UiLayout::CreativePageSize);++index){auto r=UiLayout::creativeItem(index,1280,720);auto hit=UiLayout::hit(UiMode::Creative,r.x+5,r.y+5,1280,720,creativeCatalog().size(),0,0,page);check(hit.kind==UiSlotKind::CreativeItem && hit.index==index,"creative_page_"+std::to_string(index));}
 check(int(Block::Carrot0)==167 && int(Block::Composter4)==197 && int(Item::Carrot)==174 && int(Item::VegetableStew)==205,"historical_id_sentinels");
 for(int id=0;id<int(Item::Count);++id){Item decoded;check(itemFromSaveId(std::uint16_t(id),decoded)&&int(decoded)==id,"item_id_"+std::to_string(id));}
 std::vector<Block> states;
 for(int id=0;id<int(Block::Count);++id){Block decoded;check(blockFromSaveId(unsigned(id),decoded)&&int(decoded)==id,"block_id_"+std::to_string(id));}
 for(const auto& d:buildingContent())for(unsigned prop=0;prop<1024;++prop){Block b=blockState(d.block,std::uint16_t(prop));if(!validBlockState(b))continue;states.push_back(b);auto shape=buildingBoxes(b);check(shape.count>0 && shape.count<=24,"shape_"+std::to_string(unsigned(b)));for(int i=0;i<shape.count;++i){auto box=shape.boxes[i];if(glm::any(glm::greaterThanEqual(box.minimum,box.maximum)))throw std::runtime_error("degenerate box");}for(int face=0;face<6;++face)if(blockTexture(b,face)<0||blockTexture(b,face)>=BlockAtlasTiles)throw std::runtime_error("texture overflow");}
 // Independent facing-vector projection: the pillow's v=0 edge must point
 // toward the head for every facing/color, including negative chunks.
 for(int chunk:{0,-1})for(int color=0;color<4;++color)for(int facing=0;facing<4;++facing)for(bool head:{false,true}){
  World::MeshInput input;input.x=chunk;input.z=chunk;input.meshHeight=8;input.buildingShapes=true;input.blocks.assign(22*22*8,Block::Air);input.packedLight.assign(input.blocks.size(),0xf0);
  input.blocks[(4*22+11)*22+11]=blockState(Block::Bed,facing|(head?16:0)|(color<<8));auto mesh=w.buildMesh(input);const glm::vec3 origin(chunk*16+8,4,chunk*16+8);const auto direction=glm::vec3(buildingFacing(facing));const glm::vec3 right(-direction.z,0,direction.x);int tops=0;bool correct=true;
  for(auto v:mesh.opaque)if(v.normal.y>.9f && std::abs(v.position.y-4.5625f)<.001f){++tops;const auto local=v.position-origin-glm::vec3(.5f,0,.5f);const glm::vec2 expected(.5f+glm::dot(local,right),.5f-glm::dot(local,direction));correct=correct && glm::length(v.uv-expected)<.0001f && int(float(v.atlasTile))==(head?170:181)+color;}
  check(tops==6 && correct,"bed_texture_facing_"+std::to_string(chunk)+"_"+std::to_string(color)+"_"+std::to_string(facing)+"_"+std::to_string(head));
 }
 World::MeshInput meshInput;meshInput.meshHeight=8;meshInput.buildingShapes=true;meshInput.blocks.assign(22*22*8,Block::Air);meshInput.packedLight.assign(meshInput.blocks.size(),0xf0);const auto meshIndex=(4*22+11)*22+11;
 for(Block state:states){meshInput.blocks[meshIndex]=state;auto mesh=w.buildMesh(meshInput);check(!mesh.opaque.empty()||!mesh.water.empty(),"renderable_state_"+std::to_string(unsigned(state)));w.packMeshOutput(mesh);}
 meshInput.blocks[meshIndex]=Block::StonePaving;meshInput.packedLight[meshIndex]=0;auto litMesh=w.buildMesh(meshInput);bool topLit=false;for(auto v:litMesh.opaque)if(v.normal.y==1 && v.sunLight>.9f)topLit=true;check(topLit,"opaque_surface_samples_neighbor_sunlight");
 std::fill(meshInput.packedLight.begin(),meshInput.packedLight.end(),0);meshInput.packedLight[meshIndex+22*22]=15;litMesh=w.buildMesh(meshInput);bool torchLit=false;for(auto v:litMesh.opaque)if(v.normal.y==1 && v.blockLight>.9f)torchLit=true;check(torchLit,"opaque_surface_samples_neighbor_torchlight");
 check(AmbientOcclusion::contribution(Block::GlassPane,{.5f,.5f,.5f})==0,"transparent_pane_no_opaque_ao");
 Block invalid;Item invalidItem;check(!blockFromSaveId(0xffffffff,invalid)&&!itemFromSaveId(65535,invalidItem),"invalid_ids_rejected");
 check(!validBlockState(blockState(Block::OakStairs,0x8000)) && !validBlockState(blockState(Block::OakSlab,12)),"invalid_properties_rejected");
 for(std::size_t i=0;i<states.size();++i)w.setBlockInternal(int(i%64),304,int(i/64),states[i],true,false);
 check(w.saveWorld("state-world.vxw",g.player_->position()),"all_states_save");World restored(991,22);glm::vec3 location;
 check(restored.loadWorld("state-world.vxw",location) && restored.edits_==w.edits_,"all_states_exact_roundtrip");
 {World encoded(991,22);for(std::size_t i=0;i<states.size();++i)encoded.edits_[encoded.chunkKey(int(i/256),-2)][encoded.localIndex(int(i%16),301,int((i/16)%16))]=states[i];check(encoded.saveWorld("each-state-codec.vxw",g.player_->position()),"every_valid_state_codec_save");World decoded(991,22);check(decoded.loadWorld("each-state-codec.vxw",location)&&decoded.edits_==encoded.edits_,"every_valid_state_codec_exact_roundtrip");}
 std::string corrupt=bytes("state-world.vxw");if(corrupt.size()>52){corrupt[52]=char(255);corrupt[53]=char(255);corrupt[54]=char(255);corrupt[55]=char(255);}write("corrupt.vxw",corrupt);auto before=restored.edits_;check(!restored.loadWorld("corrupt.vxw",location)&&restored.edits_==before,"invalid_state_transactional");
 // Connections and real occupied volumes at a negative chunk boundary.
 for(int x=-2;x<=2;++x)w.setBlock(x,300,-3,Block::Air);
 w.setBlock(-1,300,-3,Block::OakFence);w.setBlock(0,300,-3,Block::BirchFence);
 check((blockProperties(w.getBlock(-1,300,-3))&32) && (blockProperties(w.getBlock(0,300,-3))&128),"fence_negative_seam_connect");
 check(w.aabbIntersectsSolid({-.6f,301.1f,-2.6f},{-.4f,301.4f,-2.4f}),"fence_effective_height");
 w.setBlock(0,300,-3,Block::Air);check(!(blockProperties(w.getBlock(-1,300,-3))&32),"fence_disconnect");
 w.setBlock(15,300,-3,Block::GlassPane);w.setBlock(16,300,-3,Block::GlassPane);check((blockProperties(w.getBlock(15,300,-3))&32)!=0,"pane_positive_seam_connect");
 w.setBlock(4,300,-3,Block::OakStairs);w.setBlock(4,300,-4,blockState(Block::BirchStairs,1));check(((blockProperties(w.getBlock(4,300,-3))>>4)&7)==4,"outer_stair_corner");
 w.setBlock(4,300,-4,Block::Air);w.setBlock(4,300,-2,blockState(Block::BirchStairs,3));check(((blockProperties(w.getBlock(4,300,-3))>>4)&7)==1,"inner_stair_corner");
 w.setBlock(8,300,-3,Block::OakStairs);check(!w.blockIntersectsAabb({8,300,-3},{8.1f,300.6f,-2.9f},{8.3f,300.8f,-2.7f}),"stair_empty_quarter_no_collision");
 RayHit hit;check(!w.raycast({8.2f,300.75f,-4},{0,0,1},1.3f,hit),"stair_empty_quarter_no_selection");
 // Production interaction and consumption, with invalid multi-block requests.
 g.setGameMode(GameMode::Survival);mobs.animals_.clear();inv.clear();inv.add(Item::OakSlab,3);
 auto target=[&](glm::ivec3 p,glm::ivec3 normal, float distance=3){g.interaction_.hasBlockTarget=true;g.interaction_.mobTarget={};g.interaction_.blockTarget={p,p+normal,normal,distance};};
 // Actual RMB input traverses shield priority, hand selection and tool wear.
 const glm::ivec3 toolTarget(3,299,6);
 auto hands=[&](Item main,Item off,int durability=10){inv.clear();inv.selectSlot(0);if(off!=Item::None){inv.add(off,1,durability);inv.swapOffhand();}if(main!=Item::None)inv.add(main,1,durability);};
 auto rightClick=[&](bool sneak){g.input_.keys_.fill(false);g.input_.previousKeys_.fill(false);g.input_.keys_[GLFW_KEY_LEFT_SHIFT]=sneak;g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;target(toolTarget,{0,1,0});g.updateEquipmentInput(true);g.handleUseAction();g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.updateEquipmentInput(true);g.handleUseAction();g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.keys_.fill(false);g.player_->setBlocking(false);};
 for(auto mode:{GameMode::Survival,GameMode::Creative})for(bool off:{false,true})for(int log=0;log<6;++log){
  const Block logs[]={Block::Log,Block::BirchLog,Block::SpruceLog,Block::JungleLog,Block::AcaciaLog,Block::DarkOakLog};
  g.setGameMode(mode);hands(off?Item::IronSword:Item::IronAxe,off?Item::IronAxe:Item::Shield);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,blockState(logs[log],log%3));rightClick(false);
  check(blockType(w.getBlock(toolTarget.x,toolTarget.y,toolTarget.z))==Block(int(Block::OakStrippedLog)+log*14),"rmb_strip_"+std::to_string(log)+"_off_"+std::to_string(off));
  check((off?inv.equipment(4):inv.selectedStack()).durability==(mode==GameMode::Creative?10:9) && (off?inv.selectedStack():inv.equipment(4)).durability==10,"rmb_axe_exact_hand_wear");
 }
 for(auto mode:{GameMode::Survival,GameMode::Creative})for(bool off:{false,true}){
  g.setGameMode(mode);hands(off?Item::IronSword:Item::IronShovel,off?Item::IronShovel:Item::Shield);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,Block::Grass);rightClick(true);
  check(w.getBlock(toolTarget.x,toolTarget.y,toolTarget.z)==Block::DirtPath,"rmb_path");
  check((off?inv.equipment(4):inv.selectedStack()).durability==(mode==GameMode::Creative?10:9) && (off?inv.selectedStack():inv.equipment(4)).durability==10,"rmb_shovel_exact_hand_wear");
 }
 g.setGameMode(GameMode::Survival);
 for(auto tool:{Item::IronAxe,Item::IronShovel})for(bool off:{false,true}){
  hands(off?Item::IronSword:tool,off?tool:Item::Shield,1);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,tool==Item::IronAxe?Block::Log:Block::Dirt);rightClick(tool==Item::IronShovel);
  check((off?inv.equipment(4):inv.selectedStack()).empty() && !(off?inv.selectedStack():inv.equipment(4)).empty(),"rmb_only_acting_tool_breaks");
 }
 // The targeted action can choose the offhand even when the main tool has a
 // different right-click use. Both eligible tools still prefer the main hand.
 hands(Item::IronShovel,Item::IronAxe);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,Block::Log);rightClick(false);check(inv.selectedStack().durability==10&&inv.equipment(4).durability==9,"rmb_target_over_generic_main_use");
 hands(Item::IronAxe,Item::IronShovel);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,Block::Dirt);rightClick(true);check(w.getBlock(toolTarget.x,toolTarget.y,toolTarget.z)==Block::DirtPath&&inv.selectedStack().durability==10&&inv.equipment(4).durability==9,"rmb_shovel_target_over_main_axe");
 hands(Item::IronAxe,Item::IronAxe);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,Block::Log);rightClick(false);check(inv.selectedStack().durability==9&&inv.equipment(4).durability==10,"rmb_same_tool_main_priority");
 hands(Item::IronAxe,Item::Shield);w.setBlock(toolTarget.x,toolTarget.y,toolTarget.z,Block::Stone);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.updateEquipmentInput(true);check(g.player_->isBlocking(),"axe_shield_still_blocks_non_log");g.input_.mouseButtons_.fill(false);g.player_->setBlocking(false);
 inv.clear();inv.add(Item::OakSlab,3);
 target({2,299,2},{0,1,0});check(g.buildingUse(Item::OakSlab,false) && inv.count(Item::OakSlab)==2,"slab_placement_consumes_once");
 target({2,300,2},{0,1,0});g.buildingUse(Item::OakSlab,false);check(blockProperties(w.getBlock(2,300,2))==8 && inv.count(Item::OakSlab)==1,"double_slab_merge");check(FarmingSystem::harvest(w.getBlock(2,300,2),{2,300,2},991)[0].count==2,"double_slab_drops_two");
 inv.clear();inv.add(Item::Bed);g.player_->yaw_=-90;target({10,299,2},{0,1,0});g.buildingUse(Item::Bed,false);check(buildingIsBed(w.getBlock(10,300,2))&&buildingIsBed(w.getBlock(10,300,1))&&inv.count(Item::Bed)==0,"bed_two_blocks_one_item");
 g.player_->teleport({9.5f,300,4.5f});g.timing_.worldTime=WorldTime::MidnightSeconds;target({10,300,1},{0,1,0});g.buildingUse(Item::None,false);check(g.player_->hasPersonalBed() && g.player_->personalBed()==glm::ivec3(10,300,2) && g.timing_.worldTime==WorldTime::DefaultSeconds,"sleep_and_personal_spawn");
 check(g.player_->save("bed-player.vps",991),"bed_player_save");Player bedPlayer({0,300,0});check(bedPlayer.load("bed-player.vps",991)&&bedPlayer.hasPersonalBed(),"bed_player_load");bedPlayer.respawn(w);check(glm::length(bedPlayer.position()-glm::vec3(10,300,2))<3,"bed_respawn_nearby");
 w.setBlock(10,300,1,Block::Air);check(w.getBlock(10,300,2)==Block::Air&&!w.findBedSpawn({10,300,2}),"bed_break_head_removes_pair");
 inv.clear();inv.add(Item::OakDoor);target({12,299,2},{0,1,0});g.buildingUse(Item::OakDoor,false);check(buildingIsDoor(w.getBlock(12,300,2))&&buildingIsDoor(w.getBlock(12,301,2)),"door_two_parts");target({12,300,2},{0,0,1});g.buildingUse(Item::None,false);check((blockProperties(w.getBlock(12,300,2))&8)&&(blockProperties(w.getBlock(12,301,2))&8),"door_open_pair");
 w.setBlock(12,301,2,Block::Air);check(w.getBlock(12,300,2)==Block::Air,"door_break_upper_removes_lower");
 w.setBlock(30,300,3,Block::OakDoor);w.setBlock(30,301,3,blockState(Block::OakDoor,16));auto recoveredCount=w.buildingDrops_.size();w.setBlock(30,299,3,Block::Air);check(w.getBlock(30,300,3)==Block::Air&&w.getBlock(30,301,3)==Block::Air&&w.buildingDrops_.size()==recoveredCount+1,"door_support_loss_one_recovery_item");
 w.setBlock(31,300,3,Block::DirtPath);w.setBlock(31,301,3,Block::Stone);check(w.getBlock(31,300,3)==Block::Dirt,"obstructed_path_reverts_to_dirt");
 // Stepping follows actual stair boxes without turning ordinary cubes into auto-jumps.
 w.setBlock(5,300,-5,Block::OakStairs);Player walker({5.5f,300,-5.8f});walker.yaw_=90;walker.grounded_=true;PlayerInput walk{};walk.enabled=true;walk.moveForward=true;float maximum=300;for(int i=0;i<120;++i){walker.update(1.f/120.f,walk,w);maximum=std::max(maximum,walker.position().y);}check(maximum>=300.99f,"stair_step_up");
 // Thin collision surfaces step without a jump, including sneak and chunk seams.
 for(const auto thin:{Block::Carpet,Block::OakPressurePlate})for(bool sneak:{false,true})for(int direction=0;direction<4;++direction){
  const glm::vec3 forward=direction==0?glm::vec3(1,0,0):direction==1?glm::vec3(-1,0,0):direction==2?glm::vec3(0,0,1):glm::vec3(0,0,-1);
  const glm::ivec3 cell(16,300,14);w.setBlock(cell.x,cell.y,cell.z,thin);
  Player p(glm::vec3(cell)+glm::vec3(.5f,0,.5f)-forward*1.3f);p.yaw_=direction==0?0.f:direction==1?180.f:direction==2?90.f:-90.f;p.grounded_=true;
  PlayerInput input{};input.enabled=true;input.moveForward=true;input.sneak=sneak;float highest=300;
  for(int frame=0;frame<300;++frame){p.update(1.f/120,input,w);highest=std::max(highest,p.position().y);}
  check((thin!=Block::Carpet || highest>=300.0624f) && glm::dot(p.position()-(glm::vec3(cell)+glm::vec3(.5f,0,.5f)),forward)>.8f,"thin_walk_"+std::to_string(unsigned(thin))+"_"+std::to_string(sneak)+"_"+std::to_string(direction));
  w.setBlock(cell.x,cell.y,cell.z,Block::Air);
 }
 for(int obstacle=0;obstacle<3;++obstacle){
  w.setBlock(-1,300,14,obstacle==1?Block::Stone:Block::Carpet);
  if(obstacle==2)w.setBlock(-1,301,14,blockState(Block::OakTrapdoor,4));
  Player p({-1.8f,300,14.5f});p.grounded_=obstacle!=0;p.yaw_=0;
  if(obstacle==0){p.walkStep_=false;p.velocity_={1,0,0};p.resolveAxis(0,.6f,w);}
  else {PlayerInput input{};input.enabled=true;input.moveForward=true;for(int i=0;i<120;++i)p.update(1.f/120,input,w);}
  check(p.position().x<-1.29f && p.position().y<300.01f,obstacle==0?"thin_no_air_step":obstacle==1?"no_full_cube_auto_jump":"thin_requires_headroom");
  w.setBlock(-1,301,14,Block::Air);w.setBlock(-1,300,14,Block::Air);
 }
 // Scaffolding floors allow upward climbing and explicit sneak descent.
 w.setBlock(26,300,5,Block::Scaffold);w.setBlock(26,301,5,Block::Scaffold);Player climber({26.5f,300,5.5f});PlayerInput climb{};climb.enabled=true;climb.jump=true;for(int i=0;i<90;++i)climber.update(1.f/120.f,climb,w);check(climber.position().y>301.5f,"scaffold_upward_passage");climb.jump=false;climb.sneak=true;for(int i=0;i<90;++i)climber.update(1.f/120.f,climb,w);check(climber.position().y<301.2f,"scaffold_sneak_descent");
 // Combining a slab must not fill the player's body with a new upper half.
 w.setBlock(3,300,2,Block::OakSlab);g.player_->teleport({3.5f,300.5f,2.5f});inv.clear();inv.add(Item::OakSlab);target({3,300,2},{0,1,0});g.buildingUse(Item::OakSlab,false);check(!(blockProperties(w.getBlock(3,300,2))&8)&&inv.count(Item::OakSlab)==1,"slab_merge_blocks_player_overlap");g.player_->teleport({9.5f,300,4.5f});
 // Medium paintings share one art and one recovery item across both blocks.
 w.setBlock(20,300,5,blockState(Block::Painting,32));w.setBlock(21,300,5,blockState(Block::Painting,32|16));w.setBlock(21,300,5,Block::Air);check(w.getBlock(20,300,5)==Block::Air,"medium_painting_pair_removal");
 w.setBlock(22,303,5,Block::Stone);w.setBlock(22,302,5,Block::Chain);w.setBlock(22,301,5,blockState(Block::Lantern,4));w.setBlock(22,303,5,Block::Air);check(w.getBlock(22,302,5)==Block::Air&&w.getBlock(22,301,5)==Block::Air,"chain_lantern_support_cascade");
 w.setBlock(24,300,5,Block::Stone);w.setBlock(24,300,6,blockState(Block::Torch,3));check(isTorch(w.getBlock(24,300,6))&&blockLightEmission(w.getBlock(24,300,6))==15,"wall_torch_emits");w.setBlock(24,300,5,Block::Air);check(w.getBlock(24,300,6)==Block::Air,"wall_torch_support_removal");
 inv.clear();inv.add(Item::Scaffold,7);check(inv.save("wide-items.vxi",991),"extended_inventory_save");Inventory highItems;check(highItems.load("wide-items.vxi",991)&&highItems.count(Item::Scaffold)==7,"extended_inventory_no_truncation");
 mobs.spawnDrop({12,300,6},Item::Scaffold,3);check(mobs.save("wide-drops.vxm",991),"extended_drop_save");SurvivalWorld highDrops(991);check(highDrops.load("wide-drops.vxm",991)&&highDrops.drops_.back().stack.item==Item::Scaffold,"extended_drop_no_truncation");
 w.setBlock(14,300,2,Block::Barrel);auto* barrel=w.chestAt({14,300,2},true);barrel->slots[0]={Item::DiamondSword,1,31};barrel->slots[1]={Item::FilledMap,1,0,88};barrel->slots[2]={Item::Scaffold,64};check(w.saveWorld("barrel.vxw",g.player_->position()),"barrel_save");World barrelLoad(991,22);barrelLoad.loadWorld("barrel.vxw",location);barrelLoad.prepareSpawnTerrain({14,300,2});auto* loadedBarrel=barrelLoad.chestAt({14,300,2});check(loadedBarrel&&loadedBarrel->slots[0].durability==31&&loadedBarrel->slots[1].identity==88&&loadedBarrel->slots[2].item==Item::Scaffold&&loadedBarrel->slots[2].count==64,"barrel_contents_roundtrip");
 g.player_->teleport({14.5f,300,4.5f});target({14,300,2},{0,0,1});g.buildingUse(Item::None,false);check(g.ui_.chestOpen()&&g.ui_.validateOpenBlock(w,*g.player_,inv),"barrel_uses_container_gui");inv.clear();
 // Like the Creative fixture, inject through GLFW's virtual cursor. A hidden,
 // unfocused desktop cursor may ignore warps or race the physical mouse.
 glfwShowWindow(g.window_);glfwFocusWindow(g.window_);glfwPollEvents();g.input_.setCursorCaptured(true);
 auto guiClick=[&](UiRect rect){int W,H,ww,wh;glfwGetFramebufferSize(g.window_,&W,&H);glfwGetWindowSize(g.window_,&ww,&wh);glfwSetCursorPos(g.window_,(rect.x+rect.width*.5)*ww/W,(rect.y+rect.height*.5)*wh/H);g.input_.keys_.fill(false);g.input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_LEFT]=true;g.ui_.updateInventoryInteraction(g.input_,W,H,inv,mobs,w,*g.player_,false);g.input_.mouseButtons_.fill(false);g.ui_.updateInventoryInteraction(g.input_,W,H,inv,mobs,w,*g.player_,false);};
 int bw,bh;glfwGetFramebufferSize(g.window_,&bw,&bh);guiClick(UiLayout::chestSlot(2,bw,bh));check(inv.count(Item::Scaffold)==64&&barrel->slots[2].empty(),"barrel_shift_click_withdraw");guiClick(UiLayout::playerSlot(UiMode::Chest,0,bw,bh));check(inv.count(Item::Scaffold)==0&&barrel->slots[2].count==64,"barrel_shift_click_deposit");inv.add(Item::Stone,Inventory::TotalSlots*64);guiClick(UiLayout::chestSlot(0,bw,bh));check(barrel->slots[0].item==Item::DiamondSword&&barrel->slots[0].durability==31&&inv.count(Item::DiamondSword)==0,"barrel_full_inventory_retains_gear");g.ui_.closeGameplayInterface(inv);g.input_.keys_.fill(false);inv.clear();g.player_->teleport({9.5f,300,4.5f});
 w.setBlock(9,300,5,Block::OakSign);check(w.setBuildingText({9,300,5},"VOXEL FRONTIER\nBUILDING TEST"),"sign_text_valid");check(!w.setBuildingText({9,300,5},std::string(25,'A')) && !w.setBuildingText({9,300,5},"a\nb\nc\nd\ne"),"sign_text_bounds");
 target({9,300,5},{0,1,0});g.buildingUse(Item::None,false);check(g.signEditorOpen_,"sign_editor_open");g.input_.typedCharacters_="!";g.input_.keys_[GLFW_KEY_ENTER]=true;g.input_.previousKeys_[GLFW_KEY_ENTER]=false;g.buildingInput();check(!g.signEditorOpen_&&w.buildingText({9,300,5}).find('!')!=std::string::npos,"sign_editor_save");g.input_.keys_.fill(false);
 w.setBlock(8,300,5,blockState(Block::ItemFrame,2));w.setBlock(8,300,4,Block::Stone);inv.clear();inv.add(Item::FilledMap,1,0,91);target({8,300,5},{0,0,1});g.buildingUse(Item::FilledMap,false);check(inv.count(Item::FilledMap)==0 && w.chestAt({8,300,5})->slots[0].identity==91,"frame_owns_map_identity");g.buildingUse(Item::None,false);check((blockProperties(w.getBlock(8,300,5))>>8)==1,"frame_rotation");
 g.input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;g.buildingUse(Item::None,false);g.input_.keys_.fill(false);check(w.chestAt({8,300,5})->slots[0].empty() && mobs.drops_.back().stack.identity==91,"frame_remove_preserves_identity");
 g.setGameMode(GameMode::Creative);target({8,300,5},{0,0,1});inv.clear();inv.add(Item::FilledMap,1,0,92);g.buildingUse(Item::FilledMap,false);check(inv.count(Item::FilledMap)==0&&w.chestAt({8,300,5})->slots[0].identity==92,"creative_frame_transfers_actual_owned_stack");
 w.flushBuildingDrops(mobs);const auto savedDrops=mobs.drops_;mobs.drops_.resize(SaveValidation::MaxDrops,mobs.drops_.back());g.input_.keys_[GLFW_KEY_LEFT_SHIFT]=true;g.buildingUse(Item::None,false);g.input_.keys_.fill(false);check(w.chestAt({8,300,5})->slots[0].identity==92&&mobs.drops_.size()==SaveValidation::MaxDrops,"full_drop_limit_retains_display_contents");
 w.retainBuildingDrop({8.5f,300.5f,5.5f},{Item::FilledMap,1,0,93});w.flushBuildingDrops(mobs);check(w.buildingDrops_.size()==1,"full_drop_limit_defers_support_recovery");
 check(w.saveWorld("pending-drops.vxw",g.player_->position()),"pending_drop_save");{World loaded(991,22);check(loaded.loadWorld("pending-drops.vxw",location)&&loaded.buildingDrops_.size()==1&&loaded.buildingDrops_[0].second.identity==93,"pending_drop_identity_roundtrip");std::unordered_set<std::uint32_t> refs;loaded.mapReferences(refs);check(refs.count(93),"pending_drop_keeps_map_reference");mobs.drops_=savedDrops;loaded.flushBuildingDrops(mobs);check(loaded.buildingDrops_.empty()&&mobs.drops_.back().stack.identity==93,"pending_drop_delivered_once_after_capacity");}w.buildingDrops_.clear();g.setGameMode(GameMode::Survival);
 w.setBlock(7,300,5,Block::FlowerPot);inv.clear();inv.add(Item::RedFlower);target({7,300,5},{0,1,0});g.buildingUse(Item::RedFlower,false);check(inv.count(Item::RedFlower)==0&&w.chestAt({7,300,5})->slots[0].item==Item::RedFlower,"pot_owned_plant");
 {std::vector<RenderItemSprite> sprites;w.renderBuildingItems(sprites,{7,301,5});bool found=false;for(auto sprite:sprites)if(sprite.item==Item::RedFlower){found=true;check(sprite.worldPlant && sprite.fixed && std::abs(sprite.center.y-sprite.size*.5f-300.5f)<.001f,"pot_world_texture_and_soil_alignment");}check(found,"pot_in_sparse_render_batch");}

 w.setBlock(6,300,5,Block::Lantern);check(blockLightEmission(w.getBlock(6,300,5))==14,"lantern_emission");w.setBlock(6,299,5,Block::Air);w.flushBuildingDrops(mobs);check(w.getBlock(6,300,5)==Block::Air,"lantern_support_loss");
 check(g.saveAll(true),"background_building_checkpoint");w.setBuildingText({9,300,5},"LATEST SIGN!");w.chestAt({14,300,2})->slots[2].count=61;check(g.saveAll(true),"queued_building_checkpoint");g.pollSave(true);
 check(g.saveAll(),"coordinated_building_checkpoint");g.returnToMainMenu();g.playSavedWorld(selected);check(g.world_&&g.world_->buildingText({9,300,5})=="LATEST SIGN!"&&g.world_->chestAt({14,300,2})->slots[2].count==61,"sign_checkpoint_reload");
 std::string recipeReport;bool recipePassed=Inventory::runCraftingSelfTest(recipeReport);check(recipePassed,"all_recipes:"+recipeReport);
 std::ofstream ids("building-ids.csv");ids<<"name,block_id,item_id,shape,material_id\n";for(const auto& d:buildingContent())ids<<d.name<<','<<unsigned(d.block)<<','<<unsigned(d.item)<<','<<int(d.shape)<<','<<unsigned(d.material)<<'\n';
 std::ofstream recipes("building-recipes.csv");recipes<<"item_id,name,output_count,table,ingredients\n";for(const auto& r:craftingRecipes())if(r.output>=Item::OakStairs){recipes<<unsigned(r.output)<<','<<itemDefinition(r.output).displayName<<','<<r.count<<','<<(r.w>2||r.h>2)<<',';for(auto ingredient:r.cells)if(!ingredient.empty())recipes<<(ingredient.group==IngredientGroup::Planks?"Any planks":ingredient.group==IngredientGroup::Logs?"Any log":itemDefinition(ingredient.item).displayName)<<';';recipes<<'\n';}
 std::cout<<"Building assertions="<<count<<" states="<<states.size()<<std::endl;g.shutdown();return 0;
}
int BuildingTest::visual(Game& g,const std::filesystem::path& output){
 std::filesystem::create_directories(output);auto& w=*g.world_;w.generate(8,{16,130,16});const int y=w.terrainHeight(12,12)+5;
 for(int x=-16;x<=64;x+=16)for(int z=-16;z<=80;z+=16)w.prepareSpawnTerrain({float(x),float(y),float(z)});
 auto block=[&](int x,int h,int z,Block b){w.setBlockInternal(x,y+h,z,b,true,false);};
 auto fill=[&](int x,int h,int z,int X,int H,int Z,Block b){for(int a=x;a<=X;++a)for(int c=z;c<=Z;++c)for(int v=h;v<=H;++v)block(a,v,c,b);};
 fill(-16,-2,-16,64,-1,80,Block::Dirt);fill(-16,0,-16,64,0,80,Block::Grass);fill(-16,1,-16,64,15,80,Block::Air);
 // A complete house with a steep roof, corner beams, glazed windows and an interior.
 fill(0,0,0,10,0,10,Block::Planks);
 for(int h=1;h<=4;++h){for(int x=0;x<=10;++x){block(x,h,0,Block::Planks);block(x,h,10,Block::Planks);}for(int z=0;z<=10;++z){block(0,h,z,Block::Planks);block(10,h,z,Block::Planks);}}
 for(int x:{0,10})for(int z:{0,10})fill(x,1,z,x,5,z,Block::OakStrippedLog);
 for(int x:{2,3,7,8})for(int h:{2,3}){block(x,h,10,Block::GlassPane);block(x,h,0,Block::GlassPane);}
 for(int z:{2,3,7,8})for(int h:{2,3}){block(0,h,z,Block::GlassPane);block(10,h,z,Block::GlassPane);}
 block(5,1,10,blockState(Block::OakDoor,2|8));block(5,2,10,blockState(Block::OakDoor,2|8|16));
 for(int x=-1;x<=11;++x)for(int z=-1;z<=11;++z){int roof=5+std::min(x+1,11-x);block(x,roof,z,blockState(Block::SpruceStairs,x<5?3:1));}
 for(int x=1;x<10;++x)for(int h=5;h<5+std::min(x+1,11-x);++h){block(x,h,0,Block::Planks);block(x,h,10,Block::Planks);}
 block(2,1,2,Block::Bed);block(2,1,1,blockState(Block::Bed,16));block(8,1,2,Block::Barrel);auto* c=w.chestAt({8,y+1,2},true);c->slots[0]={Item::Bread,12};c->slots[1]={Item::IronAxe,1,170};
 block(5,1,4,Block::OakTable);block(5,1,5,Block::OakChair);block(5,1,3,blockState(Block::OakChair,2));block(8,2,2,Block::FlowerPot);w.chestAt({8,y+2,2},true)->slots[0]={Item::RedFlower,1};
 block(3,1,6,blockState(Block::Carpet,256));block(4,1,6,blockState(Block::Carpet,256));block(5,1,6,blockState(Block::Carpet,256));block(3,4,3,blockState(Block::Lantern,4));block(7,4,7,blockState(Block::Lantern,4));block(3,1,8,Block::Lantern);
 block(2,2,0,Block::Planks);block(3,2,0,Block::Planks);block(7,2,0,Block::Planks);block(3,2,1,blockState(Block::Painting,2|32));block(2,2,1,blockState(Block::Painting,2|32|16));block(7,2,1,blockState(Block::OakSign,2|16));w.setBuildingText({7,y+2,1},"VOXEL FRONTIER\nHOME SWEET HOME");
 // Masonry walls and four crenellated towers.
 fill(28,0,0,48,0,20,Block::StonePaving);
 for(int h=1;h<=6;++h){for(int x=28;x<=48;++x){block(x,h,0,Block::StoneBricks);block(x,h,20,Block::StoneBricks);}for(int z=0;z<=20;++z){block(28,h,z,Block::StoneBricks);block(48,h,z,Block::StoneBricks);}}
 fill(36,1,20,40,4,20,Block::Air);for(int x=28;x<=48;x+=2){block(x,7,0,Block::StoneBricksWall);block(x,7,20,Block::StoneBricksWall);}
 for(int x:{28,46})for(int z:{0,18}){fill(x,1,z,x+2,9,z+2,Block::ChiseledStoneBricks);for(int a=x;a<=x+2;++a)for(int b=z;b<=z+2;++b)if(a==x||a==x+2||b==z||b==z+2)block(a,10,b,Block::StoneBricksWall);block(x+1,10,z+1,Block::Lantern);}
 // A working crop plot, fence pen and barn porch.
 for(int x=0;x<=16;++x){block(x,1,25,Block::OakFence);block(x,1,40,Block::OakFence);}for(int z=25;z<=40;++z){block(0,1,z,Block::OakFence);block(16,1,z,Block::OakFence);}block(8,1,40,blockState(Block::OakFenceGate,0));
 for(int x=2;x<=14;++x)for(int z=27;z<=37;++z){block(x,0,z,x==8?Block::Water:Block::WetFarmland);if(x!=8)block(x,1,z,x<8?Block::Crop3:Block::Carrot3);}
 fill(-5,1,26,-5,3,26,Block::SpruceStrippedLog);fill(-5,1,38,-5,3,38,Block::SpruceStrippedLog);block(-5,4,26,Block::Lantern);block(-5,4,38,Block::Lantern);fill(-8,0,26,-4,0,38,Block::SprucePlanks);fill(-8,4,25,-4,4,39,blockState(Block::SpruceSlab,4));
 // Pond and timber bridge with connected rails.
 fill(27,-1,25,52,0,40,Block::Water);for(int x=28;x<=50;++x)for(int z=31;z<=34;++z)block(x,1,z,Block::OakSlab);
 for(int x=28;x<=50;++x){block(x,2,31,Block::OakFence);block(x,2,34,Block::OakFence);}for(int x:{28,35,43,50})for(int z:{31,34}){block(x,0,z,Block::OakStrippedLog);block(x,3,z,Block::Lantern);}
 // Explicit examples of every corner and slab half, with varied stone profiles.
 for(int f=0;f<4;++f)for(int shape=0;shape<5;++shape){glm::ivec3 p(f*3,1,52+shape*3);block(p.x,p.y,p.z,blockState(Block::BirchStairs,f));if(shape){auto neighbor=p+(shape>=3?buildingFacing(f):-buildingFacing(f));const auto turn=(shape==1||shape==3)?3:1;block(neighbor.x,neighbor.y,neighbor.z,blockState(Block::BirchStairs,(f+turn)&3));}}
 for(int i=0;i<11;++i){auto b=Block(int(Block::CobblestoneStairs)+i*3);block(18+i*2,1,54,b);block(18+i*2,1,57,Block(int(b)+2));}
 for(int cx=-1;cx<=4;++cx)for(int cz=-1;cz<=5;++cz)w.refreshBuildingConnections(cx,cz);
 g.setGameMode(GameMode::Spectator);g.settings_.renderDistance=8;g.timing_.worldTime=180;g.player_->setFlying(true);
 auto view=[&](const char* name,glm::vec3 p,float yaw,float pitch){g.player_->teleport(p+glm::vec3(0,y,0));g.player_->yaw_=yaw;g.player_->pitch_=pitch;auto started=glfwGetTime();while(glfwGetTime()-started<3){glfwPollEvents();w.updateStreaming(g.player_->cameraPosition(),8);g.renderFrame(.008f);std::this_thread::sleep_for(std::chrono::milliseconds(4));}g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path latest;for(const auto& f:std::filesystem::directory_iterator("screenshots"))if(latest.empty()||f.last_write_time()>std::filesystem::last_write_time(latest))latest=f.path();check(!latest.empty(),std::string("screenshot_")+name);std::filesystem::copy_file(latest,output/(std::string(name)+".bmp"),std::filesystem::copy_options::overwrite_existing);};
 block(20,1,10,Block::FlowerPot);w.chestAt({20,y+1,10},true)->slots[0]={Item::RedFlower,1};
 for(int facing=0;facing<4;++facing){glm::ivec3 foot(-10+facing*4,y+1,17),head=foot+buildingFacing(facing);w.setBlockInternal(foot.x,foot.y,foot.z,blockState(Block::Bed,facing),true,false);w.setBlockInternal(head.x,head.y,head.z,blockState(Block::Bed,facing|16),true,false);}
 view("bed-four-directions",{-3.5f,6.5f,24},-90,-45);
 view("pot-front",{20.5f,.2f,13},-90,0);view("pot-side",{23,.2f,10.5f},180,0);view("pot-back",{20.5f,.2f,8},90,0);
 view("wooden-house",{18,11,23},-126,-23);view("stone-fortress",{57,18,31},-139,-27);view("farm-and-gates",{20,8,47},-131,-25);g.timing_.worldTime=700;view("decorated-interior",{8,2,8},-128,-8);view("sign-closeup",{7.5f,1.2f,3.8f},-90,0);g.timing_.worldTime=180;view("timber-bridge",{48,7,47},-120,-17);view("shape-gallery",{25,14,76},-109,-32);
 check(g.saveAll(),"visual_checkpoint");g.shutdown();return 0;
}
int BuildingTest::benchmark(Game& g,const std::filesystem::path& output){
 std::filesystem::create_directories(output);glfwSwapInterval(0);auto& w=*g.world_;g.setGameMode(GameMode::Spectator);g.player_->teleport({64,330,124});g.player_->yaw_=-90;g.player_->pitch_=-25;g.settings_.renderDistance=8;
 for(int x=0;x<128;x+=16)for(int z=0;z<128;z+=16)w.prepareSpawnTerrain({float(x),300,float(z)});
 const Block materials[]={Block::OakStairs,Block::OakFence,Block::StoneBricksWall,Block::GlassPane,Block::StoneBricks,Block::OakSlab,Block::OakTable,Block::Barrel};
 std::ofstream csv(output/"dense-scenes.csv");csv<<"blocks,bulk_place_ms,connect_1000_ms,mesh_pack_ms,upload_ms,vertices,chunk_storage_bytes,save_bytes,save_ms,checkpoint_ms,mean_frame_ms,p95_ms,p99_ms,worst_ms,working_set_bytes,gpu_live_bytes,gpu_capacity_bytes\n";
 auto now=[] {return std::chrono::steady_clock::now();};auto elapsed=[&](auto start){return std::chrono::duration<double,std::milli>(now()-start).count();};
 for(int n:{1000,10000,100000}){
  auto start=now();std::set<std::pair<int,int>> touched;
  for(int i=0;i<n;++i){int x=i%128,z=(i/128)%128,y=300+i/(128*128);auto* chunk=w.findChunk(x/16,z/16);if(!chunk)throw std::runtime_error("Dense fixture chunk unloaded");Block b=materials[(i/4)%8];chunk->setLocal(x%16,y,z%16,b);w.edits_[w.chunkKey(x/16,z/16)][w.localIndex(x%16,y,z%16)]=b;if(b==Block::Barrel)w.chestAt({x,y,z},true)->slots[0]={Item::Stone,16};touched.insert({x/16,z/16});}
  double placement=elapsed(start);start=now();for(int i=0;i<1000;++i)w.setBlock(i%128,300,(i/128)%128,(i&1)?Block::OakFence:Block::GlassPane);double connected=elapsed(start);
  double meshMs=0,uploadMs=0;std::size_t vertices=0,storage=0;
  for(auto [cx,cz]:touched){auto* c=w.findChunk(cx,cz);c->rebuildLightEmitterIndex();w.computeSunlight(*c);w.markDirty(cx,cz,true);auto input=w.captureMeshInput(cx,cz);start=now();auto mesh=w.buildMesh(input);vertices+=mesh.opaque.size()+mesh.water.size();w.packMeshOutput(mesh);meshMs+=elapsed(start);start=now();w.uploadMeshResult(mesh);uploadMs+=elapsed(start);storage+=c->blocks.storageBytes();}
  start=now();auto path=output/("dense-"+std::to_string(n)+".vxw");check(w.saveWorld(path.string(),g.player_->position()),"dense_save");double saveMs=elapsed(start);{World reloaded(991,22);glm::vec3 p;check(reloaded.loadWorld(path.string(),p)&&reloaded.edits_==w.edits_,"dense_exact_edit_reload");}start=now();check(g.saveAll(),"dense_coordinated_checkpoint");double checkpointMs=elapsed(start);
  auto settle=now();while(elapsed(settle)<1000){glfwPollEvents();w.updateStreaming(g.player_->cameraPosition(),8);g.renderFrame(.008f);}
  std::vector<double> frames;for(int frame=0;frame<360;++frame){glfwPollEvents();auto before=now();g.renderFrame(1.f/120);frames.push_back(elapsed(before));}
  std::sort(frames.begin(),frames.end());double mean=0;for(auto t:frames)mean+=t;mean/=frames.size();PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));auto gpu=w.terrainMemory();
  csv<<n<<','<<placement<<','<<connected<<','<<meshMs<<','<<uploadMs<<','<<vertices<<','<<storage<<','<<std::filesystem::file_size(path)<<','<<saveMs<<','<<checkpointMs<<','<<mean<<','<<frames[342]<<','<<frames[356]<<','<<frames.back()<<','<<memory.WorkingSetSize<<','<<gpu.usedBytes<<','<<gpu.capacityBytes<<std::endl;
 }
 std::ofstream timing(output/"building-timing.csv");timing<<"fps,seconds,steps,health,hunger,saturation\n";
 for(int fps:{10,15,20,30,60,120}){SimulationClock clock;Player p({.5f,300,.5f});p.setCreativeMode(true);p.health_=9;p.hunger_=12;p.saturation_=3;PlayerInput input{};int steps=0;for(int frame=0;frame<fps*10;++frame){auto ticks=clock.advance(1.0/fps);for(unsigned step=0;step<ticks;++step){p.update(float(SimulationClock::Step),input,w);++steps;}}check(p.health()==9&&p.hunger()==12&&p.saturation()==3,"timed_creative_stats");timing<<fps<<",10,"<<steps<<','<<p.health()<<','<<p.hunger()<<','<<p.saturation()<<'\n';}
 g.shutdown();return 0;
}
int BuildingTest::creativeUi(Game& g,const std::filesystem::path& output){
 std::filesystem::create_directories(output);auto& inv=*g.inventory_;g.ui_.openInventory();g.synchronizeCursorCapture();
 // Use GLFW's virtual cursor for deterministic input, without repeatedly
 // warping the user's desktop pointer or racing physical mouse movement.
 glfwFocusWindow(g.window_);glfwPollEvents();g.input_.setCursorCaptured(true);
 int W,H,ww,wh;glfwGetFramebufferSize(g.window_,&W,&H);glfwGetWindowSize(g.window_,&ww,&wh);
 auto pointer=[&](UiRect r){glfwSetCursorPos(g.window_,(r.x+r.width*.5)*ww/W,(r.y+r.height*.5)*wh/H);};
 auto update=[&]{g.ui_.updateInventoryInteraction(g.input_,W,H,inv,*g.survival_,*g.world_,*g.player_,true);};
 auto click=[&](UiRect r,bool right=false,bool shift=false){pointer(r);g.input_.keys_.fill(false);g.input_.keys_[GLFW_KEY_LEFT_SHIFT]=shift;g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[right?GLFW_MOUSE_BUTTON_RIGHT:GLFW_MOUSE_BUTTON_LEFT]=true;update();g.input_.mouseButtons_.fill(false);update();g.input_.keys_.fill(false);};
 const auto& catalog=creativeCatalog();const int pages=int((catalog.size()+UiLayout::CreativePageSize-1)/UiLayout::CreativePageSize);
 for(int page=0;page<pages;++page){
  check(g.ui_.creativePage()==page,"creative_actual_page_"+std::to_string(page));
  for(int index=page*UiLayout::CreativePageSize;index<std::min(int(catalog.size()),(page+1)*UiLayout::CreativePageSize);++index){
   auto r=UiLayout::creativeItem(index,W,H);inv.clear();click(r);
   check(inv.cursorStack().item==catalog[index]&&inv.cursorStack().count==Inventory::maxStack(catalog[index]),"creative_left_actual_item_"+std::to_string(index)+" expected="+std::to_string(int(catalog[index]))+" actual="+std::to_string(int(inv.cursorStack().item)));
   inv.clear();click(r,true);check(inv.cursorStack().item==catalog[index]&&inv.cursorStack().count==1,"creative_right_actual_item_"+std::to_string(index));
   inv.clear();click(r,false,true);int slot=catalog[index]==Item::Shield?Inventory::OffhandSlot:armorSlot(catalog[index]);check(slot>=0?inv.equipment(slot).item==catalog[index]:inv.cursorStack().item==catalog[index],"creative_shift_actual_item_"+std::to_string(index));
   check(InventoryTooltip::hoveredItem(UiMode::Creative,W,H,r.x+5,r.y+5,inv,nullptr,nullptr,nullptr,page)==catalog[index],"creative_tooltip_item_"+std::to_string(index));
  }
  inv.clear();const int first=page*UiLayout::CreativePageSize;
  pointer(UiLayout::creativeItem(first,W,H));g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=false;update();
  pointer(UiLayout::creativeItem(first+1,W,H));g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;update();check(inv.cursorStack().item==catalog[first+1]&&inv.cursorStack().count==1,"creative_held_right_drag_page_"+std::to_string(page));g.input_.mouseButtons_.fill(false);update();g.input_.previousMouseButtons_.fill(false);inv.clear();
  if(page==pages-1 && int(catalog.size())<(page+1)*UiLayout::CreativePageSize){click(UiLayout::creativeItem(int(catalog.size()),W,H));check(inv.cursorStack().empty(),"creative_empty_last_page_cell");}
  g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path latest;for(const auto& f:std::filesystem::directory_iterator("screenshots"))if(latest.empty()||f.last_write_time()>std::filesystem::last_write_time(latest))latest=f.path();std::filesystem::copy_file(latest,output/("creative-page-"+std::to_string(page+1)+".bmp"),std::filesystem::copy_options::overwrite_existing);
  if(page+1<pages){auto p=UiLayout::panel(UiMode::Creative,W,H);click({p.x+550,p.y+475,40,30});}
 }
 for(auto size:{glm::ivec2(1280,720),glm::ivec2(1024,768),glm::ivec2(800,700)})for(int index=0;index<int(catalog.size());++index){auto r=UiLayout::creativeItem(index,size.x,size.y),owned=UiLayout::playerSlot(UiMode::Creative,0,size.x,size.y);check(r.width==owned.width&&r.height==owned.height,"creative_slot_matches_inventory_"+std::to_string(index));}
 // Filtered catalog indices must stay aligned across draw, tooltip, clicks,
 // paging and right-drag, including items from the original last page.
 auto typeSearch=[&](const std::string& query){click(UiLayout::creativeSearch(W,H));g.input_.keys_[GLFW_KEY_LEFT_CONTROL]=true;g.input_.keys_[GLFW_KEY_A]=true;g.input_.previousKeys_.fill(false);update();g.input_.keys_.fill(false);g.input_.typedCharacters_=query;update();};
 for(const auto& query:{std::string("STONE"),std::string("oak"),std::string("a"),std::string("zzzzzz")}){
  typeSearch(query);check(g.ui_.creativePage()==0,"creative_search_resets_page");
  std::string lower=query;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
  std::vector<Item> expected;for(auto item:catalog){std::string name=itemDefinition(item).displayName;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});if(name.find(lower)!=std::string::npos)expected.push_back(item);}
  check(g.ui_.creativeItems()==expected,"creative_search_exact_results_"+query);
  int filteredPages=std::max(1,(int(expected.size())+UiLayout::CreativePageSize-1)/UiLayout::CreativePageSize);
  for(int page=0;page<filteredPages;++page){
   for(int i=page*UiLayout::CreativePageSize;i<std::min(int(expected.size()),(page+1)*UiLayout::CreativePageSize);++i){
    auto r=UiLayout::creativeItem(i,W,H);inv.clear();click(r);check(inv.cursorStack().item==expected[i],"filtered_left_correct_item");inv.clear();click(r,true);check(inv.cursorStack().item==expected[i]&&inv.cursorStack().count==1,"filtered_right_correct_item");
    check(InventoryTooltip::hoveredItem(UiMode::Creative,W,H,r.x+5,r.y+5,inv,nullptr,nullptr,nullptr,page,&g.ui_.creativeItems())==expected[i],"filtered_tooltip_correct_item");
   }
   if(page+1<filteredPages){auto p=UiLayout::panel(UiMode::Creative,W,H);click({p.x+550,p.y+475,40,30});check(g.ui_.creativePage()==page+1,"filtered_next_page");}
  }
  if(expected.empty()){inv.clear();click(UiLayout::creativeItem(0,W,H));check(inv.cursorStack().empty(),"empty_search_no_ghost_item");}
 }
 typeSearch("oak");inv.clear();pointer(UiLayout::creativeItem(0,W,H));g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_.fill(false);update();pointer(UiLayout::creativeItem(1,W,H));g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;update();check(inv.cursorStack().item==g.ui_.creativeItems()[1],"filtered_right_drag_correct_item");g.input_.mouseButtons_.fill(false);update();
 typeSearch("stone");inv.clear();g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path searchShot;for(const auto& f:std::filesystem::directory_iterator("screenshots"))if(searchShot.empty()||f.last_write_time()>std::filesystem::last_write_time(searchShot))searchShot=f.path();std::filesystem::copy_file(searchShot,output/"creative-search.bmp",std::filesystem::copy_options::overwrite_existing);
 inv.clear();inv.add(Item::Stone,5);pointer(UiLayout::playerSlot(UiMode::Creative,0,W,H));auto drops=g.survival_->drops_.size();g.input_.keys_[GLFW_KEY_Q]=true;g.input_.previousKeys_.fill(false);g.input_.typedCharacters_="q";update();check(inv.count(Item::Stone)==5&&g.survival_->drops_.size()==drops,"search_typing_q_does_not_drop_owned_items");g.input_.keys_.fill(false);
 g.ui_.handleEscape(inv);check(!g.ui_.creativeSearchFocused()&&g.ui_.state()==GameState::Inventory,"search_escape_blurs_before_closing");
 typeSearch("");check(g.ui_.creativeItems()==catalog&&g.ui_.creativePage()==0,"search_clear_restores_all_items");
 g.input_.keys_[GLFW_KEY_ENTER]=true;g.input_.previousKeys_.fill(false);update();check(!g.ui_.creativeSearchFocused(),"search_enter_blurs");g.input_.keys_.fill(false);
 typeSearch("st");g.input_.keys_[GLFW_KEY_E]=true;g.input_.previousKeys_.fill(false);g.handleGlobalInput();check(g.ui_.state()==GameState::Inventory,"search_e_does_not_toggle_inventory");g.input_.keys_.fill(false);g.ui_.closeGameplayInterface(inv);check(!g.ui_.creativeSearchFocused(),"closing_inventory_clears_search_focus");
 std::cout<<"Creative production UI assertions="<<count<<" pages="<<pages<<" catalog="<<catalog.size()<<std::endl;g.shutdown();return 0;
}
