#ifdef _WIN32
#include <windows.h>
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "Player.h"
#include "Transportation.h"
#include "Renderer.h"
#include "SaveFile.h"
#include "Persistence.h"
#include "Screenshot.h"
#include "ExplorationLoot.h"
#include "Weather.h"
#include "Sound.h"
#include "Farming.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <future>
#include <queue>
#include <set>
#include <chrono>
#include <thread>

double fixtureWorkingSetMb();
class AdventureTest {
    static inline unsigned checks=0;
    static void check(bool ok,const std::string& name){++checks;std::cout<<"adventure "<<name<<" passed="<<ok<<std::endl;if(!ok)throw std::runtime_error(name);}
    static auto site(){return [](int,int){return UndergroundSite{120,Biome::Forest,0};};}
    static auto cave(){return [](int,int,int){return true;};}
    static auto reserved(){return [](const StructureBox&){return false;};}
    static std::shared_ptr<const UndergroundPlan> plan(World& w,RelicPuzzle kind){
        for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x){auto p=w.underground24_.plan(x,z,site(),cave(),reserved());if(p&&p->kind!=UndergroundKind::Mineshaft&&relicChamber(*p,w.seed()).kind==kind)return p;}
        throw std::runtime_error("No synthetic chamber plan");
    }
    static void loadRoom(World& w,const UndergroundPlan& plan){
        const auto c=relicChamber(plan,w.seed()).center;
        for(int z=adventureFloorDiv(c.z-8,16)-1;z<=adventureFloorDiv(c.z+8,16)+1;++z)for(int x=adventureFloorDiv(c.x-8,16)-1;x<=adventureFloorDiv(c.x+8,16)+1;++x){
            World::GeneratedChunk data;data.x=x;data.z=z;data.blocks.assign(16*16*384,Block::Stone);
            w.underground24_.apply(x,z,data.blocks,data.loot,data.mobs,site(),cave(),reserved(),-64,320);
            w.initializeGeneratedLoot(data);w.chunks_[w.chunkKey(x,z)]=std::make_unique<Chunk>(x,z,std::move(data.blocks),-64,320);
            w.retainGeneratedAutomation(x,z);
        }
        for(const auto& e:w.chunks_)w.automation.activateChunk(w,e.second->x,e.second->z);
        w.setSimulationDistance(8);
    }
    static void tick(World& w,SurvivalWorld& mobs,Player& p,Inventory& inv,float dt=.1f,int n=1){for(int i=0;i<n;++i){w.automation.update(dt,w,mobs,&p,1);w.adventure.update(dt,w,mobs,p,inv);}}
    static void review(){
        bool passed=true;auto probe=[&](bool ok,const char* name){std::cout<<"review "<<name<<" passed="<<ok<<std::endl;passed&=ok;};
        bool definitions=true;for(int i=1;i<int(Item::Count);++i){auto d=buildingDefinition(Item(i));const bool expected=(i>=int(Item::OakStairs)&&i<=int(Item::Scaffold))||(i>=int(Item::PowerWire)&&i<=int(Item::AutoHarvester));if(expected){if(!d||d->item!=Item(i))definitions=false;}else if(d)definitions=false;}
        probe(definitions,"every_item_definition");
        if(definitions)for(int i=1;i<int(Item::Count);++i){Inventory inv;inv.clear();inv.add(Item(i));inv.add(Item::Stone,2);inv.sortPlayer();probe(inv.count(Item(i))>0,"sort_item");}
        World w(991,24);auto pl=plan(w,RelicPuzzle::Plates);loadRoom(w,*pl);auto c=relicChamber(*pl,991);SurvivalWorld mobs(991);Inventory inv;inv.clear();Player player(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));
        for(int n:{0,2}){player.teleport(glm::vec3(c.control(n))+glm::vec3(.5f,2.1f,.5f));tick(w,mobs,player,inv,.1f,2);}
        probe(w.adventure.puzzles().at(Adventure::regionId(c.regionX,c.regionZ)).sequence==0,"elevated_plate_rejected");
        for(int n:{0,2}){player.teleport(glm::vec3(c.control(n))+glm::vec3(.5f,.0625f,.5f));tick(w,mobs,player,inv,.1f,2);}
        player.teleport(glm::vec3(c.center)+glm::vec3(.5f,3,.5f));player.damageInvulnerability_=0;const auto health=player.health();tick(w,mobs,player,inv,.1f,25);probe(player.health()==health,"elevated_pulse_rejected");
        inv.add(Item::SurveyorCompass);bool surveyed=false;std::string message;w.adventure.useTool(w,player,inv,false,false,message,false,&surveyed);
        surveyed=true;w.adventure.useTool(w,player,inv,false,false,message,false,&surveyed);probe(!surveyed,"retry_does_not_publish_survey_target");

        const auto base=c.center+glm::ivec3(0,9,0);auto put=[&](glm::ivec3 q,Block b){w.setBlockInternal(q.x,q.y,q.z,b,true,false);};
        for(int z=-3;z<=3;++z)for(int x=-3;x<=3;++x)for(int y=-1;y<5;++y)put(base+glm::ivec3(x,y,z),y==-1?Block::Stone:Block::Air);
        auto gate=base;put(gate,Block::OakFenceGate);put(gate+glm::ivec3(0,1,0),Block::Carpet);auto drops=w.buildingDrops_.size();
        probe(w.automationBatch({{gate,Block::OakFenceGate}},{{gate,blockState(Block::OakFenceGate,8)}})&&w.getBlock(gate.x,gate.y+1,gate.z)==Block::Air&&w.buildingDrops_.size()==drops+1,"gate_support_cleanup_retains_drop");
        put(gate,Block::OakFenceGate);put(gate+glm::ivec3(0,1,0),Block::Carpet);w.buildingDrops_.resize(100000,{glm::vec3(base),{Item::Stone,1}});
        probe(w.automationBatch({{gate,Block::OakFenceGate}},{{gate,blockState(Block::OakFenceGate,8)}})&&w.getBlock(gate.x,gate.y+1,gate.z)==Block::Carpet&&w.buildingDrops_.size()==100000,"support_cleanup_retains_owner_when_recovery_full");w.buildingDrops_.clear();put(gate,Block::Air);put(gate+glm::ivec3(0,1,0),Block::Air);
        auto piston=base+glm::ivec3(-2,0,0),stone=piston+glm::ivec3(1,0,0);put(piston,blockState(Block::Piston,1));put(stone,Block::Stone);put(stone+glm::ivec3(0,0,1),Block::RedFlower);put(stone+glm::ivec3(0,-1,1),Block::Dirt);
        probe(w.automation.piston(w,mobs,nullptr,piston,blockState(Block::Piston,1),true),"independent_flower_does_not_block_piston");
        auto furnace=base+glm::ivec3(2,0,-2);put(furnace,Block::Furnace);auto f=w.furnaceAt(furnace,true);f->input={Item::Cobblestone,2};f->fuel={Item::Coal,1};w.updateBlockEntities(.2f);f->progress=.96f;f->input={Item::IronOre,1};w.updateBlockEntities(.3f);
        probe(f->output.empty()&&f->progress<.1f,"furnace_recipe_change_resets_work");
        f->input.clear();f->progress=.8f;auto hopper=furnace+glm::ivec3(0,1,0);put(hopper,blockState(Block::Hopper,4));w.chestAt(hopper,true)->slots[0]={Item::GoldOre,1};probe(w.automation.transfer(w,hopper,blockState(Block::Hopper,4)),"hopper_supplies_changed_recipe");w.updateBlockEntities(.2f);probe(f->output.empty()&&f->progress<.1f,"hopper_recipe_does_not_inherit_progress");

        if(Persistence::enabled()){f->progress=.8f;f->input={Item::IronOre,1};auto snapshot=w.captureSaveWorld("review-furnace.vxw",player.position());probe(snapshot(),"changed_recipe_snapshot_written");World saved(991,1);glm::vec3 pos;probe(saved.loadWorld("review-furnace.vxw",pos)&&saved.furnaceAt(furnace)&&saved.furnaceAt(furnace)->progress==0,"changed_recipe_save_cannot_restore_old_work");}

        for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x){put(base+glm::ivec3(x,-1,z),Block::Cactus);put(base+glm::ivec3(x,0,z),Block::Air);put(base+glm::ivec3(x,1,z),Block::Air);}
        put(base,Block::Bed);put(base+glm::ivec3(0,0,-1),blockState(Block::Bed,16));probe(!w.findBedSpawn(base),"bed_rejects_cactus_floor");for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x)put(base+glm::ivec3(x,-1,z),Block::Log);probe(bool(w.findBedSpawn(base)),"bed_keeps_safe_constructed_log_floors");probe(!w.findBedSpawn({29900000,80,29900000},false),"bed_hint_unloaded_only");
        check(passed,"review_edge_cases");
    }
    static void conservation(const std::filesystem::path& output){
        for(auto item:{Item::SurveyorCompass,Item::EchoLantern,Item::AncientExplorerTool,Item::SurveyorToken,Item::CavernShard,Item::AncientSeal}){
            const auto& recipes=craftingRecipes();const auto it=std::find_if(recipes.begin(),recipes.end(),[&](const RecipeInfo& r){return r.output==item;});check(it!=recipes.end(),"recipe_registered");
            Inventory inv;inv.clear();for(int i=0;i<9;++i)if(!it->cells[i].empty())inv.crafting_[i]={it->cells[i].item,1,Inventory::maxDurability(it->cells[i].item)};
            check(inv.craftingOutput(true).item==item,"production_recipe_output");
            auto before=inv.crafting_;inv.cursor_={Item::Stone,64};check(!inv.takeCraftingOutput(true)&&std::equal(inv.crafting_.begin(),inv.crafting_.end(),before.begin(),[](const ItemStack& a,const ItemStack& b){return a.item==b.item&&a.count==b.count&&a.durability==b.durability&&a.identity==b.identity;}),"failed_craft_retains_ingredients");
            inv.cursor_.clear();check(inv.takeCraftingOutput(true)&&inv.cursor_.item==item&&inv.cursor_.durability==Inventory::maxDurability(item),"craft_metadata");
            check(inv.craftedRelicTools!=0,"craft_event");Adventure a;a.observeInventory(inv);check(a.crafted!=0,"journal_actual_craft");
        }
        Adventure a;check(a.discover(0,{1,64,-1})&&!a.discover(0,{1,64,-1}),"discovery_dedup");
        for(int n=0;n<35;++n){a.biome(Biome(n));a.biome(Biome(n));}check(a.biomes==((std::uint64_t(1)<<35)-1),"all_35_biomes");
        std::ostringstream out(std::ios::binary);check(a.write(out),"extension_write");const auto bytes=out.str();
        Adventure restored;std::istringstream input(bytes,std::ios::binary);check(restored.read(input)&&restored.biomes==a.biomes&&restored.discoveries().size()==1,"extension_roundtrip");
        for(std::size_t n=0;n<bytes.size();++n){Adventure untouched;untouched.biomes=4;std::istringstream bad(bytes.substr(0,n),std::ios::binary);check(!untouched.read(bad)&&untouched.biomes==4,"truncation_atomic_"+std::to_string(n));}
        for(int i=0;i<int(Adventure::MaxDiscoveries);++i)a.discover(1,{i,64,0});check(!a.discover(1,{9999,64,0})&&a.discoveries().size()==Adventure::MaxDiscoveries,"bounded_discoveries");
        if(Persistence::enabled())for(unsigned version=1;version<=23;++version){
            World original(991,version);original.adventure.biomes=1;original.adventure.relics=7;original.adventure.crafted=63;
            const auto path=(output/("legacy-relic-"+std::to_string(version)+".vxw")).string();check(original.saveWorld(path,{.5f,80,.5f}),"legacy_relic_state_write");
            World loaded(991,24);glm::vec3 position;check(loaded.loadWorld(path,position)&&loaded.generationVersion()==version&&loaded.adventure.crafted==63&&loaded.adventure.relics==7,"legacy_relic_state_retains_generator");
        }
    }
    static void chambers(Game& g,const std::filesystem::path& out){
        {World single(991,24);bool found=false;for(int z=-4;z<=4&&!found;++z)for(int x=-4;x<=4&&!found;++x){auto p=single.underground24_.plan(x,z,site(),cave(),reserved());if(!p||p->kind!=UndergroundKind::Dungeon||p->rooms.size()!=1)continue;
            const auto supplies=std::count_if(p->loot.begin(),p->loot.end(),[](auto m){return m.table==StructureLoot::RelicSupplies;}),vaults=std::count_if(p->loot.begin(),p->loot.end(),[](auto m){return m.table==StructureLoot::RelicVault;});
            check(supplies==1&&vaults==1,"one_room_dungeon_has_accessible_key_and_locked_reward");found=true;}check(found,"one_room_fixture_found");}
        for(auto kind:{RelicPuzzle::Sequence,RelicPuzzle::Routing,RelicPuzzle::Plates}){
            auto owned=std::make_unique<World>(991,24);auto& w=*owned;auto planPtr=plan(w,kind);const auto c=relicChamber(*planPtr,w.seed());loadRoom(w,*planPtr);
            Player p(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));Inventory inv;inv.clear();SurvivalWorld mobs(991);std::string message;
            check(w.getBlock(c.chest().x,c.chest().y,c.chest().z)==Block::Chest,"generated_vault_chest");
            if(kind!=RelicPuzzle::Sequence){bool visibleBand=true;for(int x=-4;x<=4;++x)visibleBand=visibleBand&&w.getBlock(c.center.x+x,c.center.y,c.center.z)==Block::TerracottaRed;check(visibleBand,"every_pulse_trial_has_visible_red_hazard_band");}
            check(w.adventure.locked(c)&&w.adventure.useTreasure(w,c,inv,p,false,message),"sealed_treasure_rejected");
            if(kind==RelicPuzzle::Sequence){
                check(w.adventure.useControl(w,c,c.control(2),message)&&w.adventure.puzzles().at(Adventure::regionId(c.regionX,c.regionZ)).sequence==0,"wrong_bell_reset");
                for(int i=0;i<3;++i)check(w.adventure.useControl(w,c,c.control(i),message),"bell_input");
                tick(w,mobs,p,inv);check(mobs.relicEncounterAlive(Adventure::regionId(c.regionX,c.regionZ)),"finite_ambush_started");const auto count=mobs.animals_.size();tick(w,mobs,p,inv,.1f,5);check(mobs.animals_.size()==count,"ambush_not_duplicated");
                if(Persistence::enabled()){
                    const auto path=(out/"ambush.vxm").string();check(mobs.save(path,991),"ambush_save");SurvivalWorld loaded(991);check(loaded.load(path,991)&&loaded.relicEncounterAlive(Adventure::regionId(c.regionX,c.regionZ)),"ambush_codec_identity");
                }
                for(std::size_t i=0;i<mobs.animals_.size();++i)if(mobs.animals_[i].relicEncounter){for(int hit=0;hit<5&&mobs.animals_[i].health>0;++hit){mobs.animals_[i].hurtCooldown=0;mobs.attackMob(int(i),Item::DiamondSword,false,p.position(),&w);}}
                tick(w,mobs,p,inv);check(!w.adventure.locked(c),"combat_unlock");
            } else if(kind==RelicPuzzle::Routing){
                for(int n:{0,2})check(w.automation.interact(w,c.control(n)),"routing_actual_switch");
                tick(w,mobs,p,inv,.1f,35);check(w.adventure.puzzles().at(Adventure::regionId(c.regionX,c.regionZ)).flags&1,"actual_AND_routing");
                check(p.health()>0&&p.health()<20,"red_band_strike_is_finite_nonlethal_damage");
                const auto beforeSafe=p.health();p.damageInvulnerability_=0;p.teleport(glm::vec3(c.center)+glm::vec3(5.5f,1,.5f));tick(w,mobs,p,inv,.1f,25);
                check(p.health()==beforeSafe,"unmarked_floor_at_band_end_is_safe");
                p.damageInvulnerability_=0;p.teleport(glm::vec3(c.center)+glm::vec3(.5f,1,-.1f));tick(w,mobs,p,inv,.1f,25);
                check(p.health()==beforeSafe,"unmarked_floor_beside_band_is_safe");
                const auto id=Adventure::regionId(c.regionX,c.regionZ);const auto remaining=w.adventure.puzzles().at(id).trialTicks;
                const int cx=adventureFloorDiv(c.center.x+7,16),cz=adventureFloorDiv(c.center.z+7,16);const auto key=w.chunkKey(cx,cz);
                auto suspended=std::move(w.chunks_.at(key));w.chunks_.erase(key);w.automation.suspendChunk(cx,cz);tick(w,mobs,p,inv,.1f,30);
                check(w.adventure.puzzles().at(id).trialTicks==remaining,"trial_pauses_at_unloaded_boundary");w.chunks_.emplace(key,std::move(suspended));w.automation.activateChunk(w,cx,cz);
                p.teleport(glm::vec3(c.center)+glm::vec3(.5f,1,8.5f));tick(w,mobs,p,inv,.1f,20);check(w.adventure.puzzles().at(id).trialTicks==remaining,"retreat_preserves_remaining_trial_ticks");
                if(Persistence::enabled()){
                    const auto path=(out/"pending-trial.vxw").string();check(w.captureSaveWorld(path,p.position())(),"pending_trial_checkpoint");World saved(991,1);glm::vec3 pos;check(saved.loadWorld(path,pos)&&saved.adventure.puzzles().at(id).trialTicks==remaining,"pending_trial_reload_does_not_restart");
                }
                p.teleport(glm::vec3(c.center)+glm::vec3(.5f,1,-1.5f));tick(w,mobs,p,inv,.1f,110);check(!w.adventure.locked(c),"routing_trial_complete");
            } else {
                for(int n:{0,2}){p.teleport(glm::vec3(c.control(n))+glm::vec3(.5f,.02f,.5f));tick(w,mobs,p,inv,.1f,2);}
                p.teleport(glm::vec3(c.center)+glm::vec3(.5f,1,-1.5f));tick(w,mobs,p,inv,.1f,110);check(!w.adventure.locked(c),"plate_trial_complete");
            }
            check(w.adventure.useTreasure(w,c,inv,p,false,message),"missing_seal_retained");inv.add(Item::AncientSeal);const auto xp=p.experience();
            check(!w.adventure.useTreasure(w,c,inv,p,false,message)&&inv.count(Item::AncientSeal)==1&&p.experience()==xp+8,"reusable_seal_reward_once");
            check(!w.adventure.useTreasure(w,c,inv,p,false,message)&&p.experience()==xp+8,"repeat_claim_no_xp");
            auto chest=w.chestAt(c.chest());check(chest!=nullptr,"finite_container_owned");
            if(chest){const auto before=chest->slots;inv.equipment_[4]={Item::AncientSeal,1};for(auto& s:inv.slots_)s={Item::Stone,64};
                check(inv.transferContainer(chest->slots.data(),27,2)==0&&std::equal(before.begin(),before.end(),chest->slots.begin(),[](const ItemStack& a,const ItemStack& b){return a.item==b.item&&a.count==b.count&&a.durability==b.durability&&a.identity==b.identity;}),"full_inventory_keeps_every_vault_stack");
                check(!w.adventure.useTreasure(w,c,inv,p,false,message)&&inv.equipment(4).count==1&&p.experience()==xp+8,"full_inventory_does_not_consume_key_or_repeat_reward");
                for(auto& s:chest->slots)s.clear();}
            if(Persistence::enabled()){
                const auto path=(out/("chamber-"+std::to_string(int(kind))+".vxw")).string();
                auto snapshot=w.captureSaveWorld(path,p.position());w.adventure.biome(Biome::Badlands);check(snapshot(),"immutable_world_snapshot");World loaded(991,1);glm::vec3 pos;check(loaded.loadWorld(path,pos)&&loaded.generationVersion()==24&&!loaded.adventure.locked(c),"checkpoint_chamber_roundtrip");
                check(loaded.chests_.at({c.chest().x,c.chest().y,c.chest().z}).slots[0].empty(),"empty_treasure_preserved");
            }
        }
        // Actual RMB hand selection, shared cooldown and wear on the same hand.
        g.world_=std::make_unique<World>(991,24);auto pl=plan(*g.world_,RelicPuzzle::Sequence);loadRoom(*g.world_,*pl);auto c=relicChamber(*pl,991);
        g.player_=std::make_unique<Player>(glm::vec3(c.center)+glm::vec3(.5f,1,-.5f));g.inventory_=std::make_unique<Inventory>();g.inventory_->clear();g.survival_=std::make_unique<SurvivalWorld>(991);g.transportation_=std::make_unique<Transportation>(g.player_->position());
        g.farming_=std::make_unique<FarmingSystem>();
        g.ui_.openInventory();g.ui_.closeGameplayInterface(*g.inventory_);g.synchronizeCursorCapture();
        {const auto q=c.control(0);g.world_->setBlock(q.x,q.y,q.z,Block::Barrel);
            const auto d=glm::normalize(glm::vec3(q)+glm::vec3(.5f)-g.player_->cameraPosition());g.player_->yaw_=glm::degrees(std::atan2(d.z,d.x));g.player_->pitch_=glm::degrees(std::asin(d.y));g.updateTargets();
            g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=false;g.handleUseAction();
            check(g.ui_.state()==GameState::Chest&&g.world_->adventure.puzzles().empty(),"mined_control_replacement_retains_normal_container_use");
            g.ui_.closeGameplayInterface(*g.inventory_);g.synchronizeCursorCapture();g.input_.mouseButtons_.fill(false);g.world_->setBlock(q.x,q.y,q.z,Block::Lever);}
        g.inventory_->add(Item::Stone,2);g.inventory_->selectSlot(0);const auto lever=c.control(0);
        {const auto d=glm::normalize(glm::vec3(lever)+glm::vec3(.5f,.25f,.5f)-g.player_->cameraPosition());g.player_->yaw_=glm::degrees(std::atan2(d.z,d.x));g.player_->pitch_=glm::degrees(std::asin(d.y));}
        g.updateTargets();check(g.interaction_.hasBlockTarget&&g.interaction_.blockTarget.block==lever,"sneak_native_target");const auto adjacent=g.interaction_.blockTarget.adjacent;
        g.input_.keys_[g.settings_.controls[int(ControlAction::Sneak)]]=true;g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=false;
        g.handleUseAction();check(g.world_->getBlock(adjacent.x,adjacent.y,adjacent.z)==Block::Stone&&g.inventory_->count(Item::Stone)==1&&g.world_->adventure.puzzles().empty()&&blockType(g.world_->getBlock(lever.x,lever.y,lever.z))==Block::Lever,"sneak_places_against_puzzle_without_consuming_progress");
        g.world_->setBlock(adjacent.x,adjacent.y,adjacent.z,Block::Air);g.input_.keys_.fill(false);g.input_.mouseButtons_.fill(false);g.inventory_->clear();
        g.inventory_->add(Item::WoodSword);g.inventory_->equipment_[4]={Item::EchoLantern,1,4};g.inventory_->selectSlot(0);
        g.player_->yaw_=-90;g.player_->pitch_=-70;g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=false;g.interaction_.hasBlockTarget=false;
        g.handleUseAction();check(g.inventory_->equipment(4).durability==3&&g.inventory_->selectedStack().durability==Inventory::maxDurability(Item::WoodSword),"actual_RMB_offhand_wear");
        g.handleUseAction();check(g.inventory_->equipment(4).durability==3,"repeated_input_cooldown");
        tick(*g.world_,*g.survival_,*g.player_,*g.inventory_,.1f,22);
        const auto editedBefore=g.world_->edits_;g.input_.keys_[g.settings_.controls[int(ControlAction::Sneak)]]=true;
        g.handleUseAction();check(g.inventory_->equipment(4).durability==2&&g.world_->edits_==editedBefore,"actual_RMB_echo_wear_no_terrain_edit");
        g.input_.keys_.fill(false);tick(*g.world_,*g.survival_,*g.player_,*g.inventory_,.1f,22);
        g.player_->teleport(g.player_->position()+glm::vec3(2,0,0));
        g.inventory_->slots_[0]={Item::EchoLantern,1,1};g.inventory_->equipment_[4]={Item::WoodSword,1,12};
        g.handleUseAction();check(g.inventory_->selectedStack().empty()&&g.inventory_->equipment(4).durability==12,"actual_RMB_mainhand_breakage");
        g.inventory_->slots_[0]={Item::SurveyorCompass,1,4};g.inventory_->equipment_[4]={Item::Shield,1,33};g.player_->setEquipment(g.inventory_.get());
        for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)if(x||z)g.world_->underground24_.plan(c.regionX+x,c.regionZ+z,site(),cave(),reserved());
        g.updateEquipmentInput(true);g.handleUseAction();check(g.inventory_->selectedStack().durability==3&&g.inventory_->equipment(4).durability==33&&!g.player_->isBlocking(),"actual_RMB_compass_shield_priority");
        check(g.transportation_->addWaypoint("Manual selection",g.player_->position()+glm::vec3(9,0,7)),"manual_waypoint_added");
        const auto manual=int(g.transportation_->waypoints().size())-1;g.transportation_->selectedWaypoint=manual;g.handleUseAction();check(g.transportation_->selectedWaypoint==manual,"recharging_RMB_compass_keeps_manual_waypoint");

        if(Persistence::enabled()){
            g.seed_=991;g.activeWorld_=WorldLibrary::create("Relic checkpoint",991,GameMode::Survival,true);g.loadedStateValid_=true;
            g.worldSession_=std::make_unique<SaveFile::Session>(g.activeWorld_.directory.string());
            for(int n=0;n<3;++n){std::string msg;g.world_->adventure.useControl(*g.world_,c,c.control(n),msg);}tick(*g.world_,*g.survival_,*g.player_,*g.inventory_);
            std::ostringstream before;g.world_->adventure.write(before);
            check(g.saveAll(true),"production_background_checkpoint_queue");
            g.world_->adventure.biome(Biome::IceSpikes);g.inventory_->add(Item::ResonantCore);
            g.pollSave(true);check(g.saveFeedback_.phase==SaveFeedback::Phase::Saved&&SaveFile::verifyCheckpoint(g.activeWorld_.directory.string()),"production_complete_verified_checkpoint");
            World loaded(991,1);glm::vec3 pos;check(loaded.loadWorld(g.worldSavePath("voxel_world.vxw"),pos),"production_checkpoint_world_load");
            std::ostringstream after;loaded.adventure.write(after);check(before.str()==after.str(),"immutable_ERL1_capture");
            SurvivalWorld savedMobs(991);check(savedMobs.load(g.worldSavePath("voxel_mobs.vxm"),991)&&savedMobs.relicEncounterAlive(Adventure::regionId(c.regionX,c.regionZ)),"coherent_ENC1_capture");
            Inventory savedInventory;check(savedInventory.load(g.worldSavePath("voxel_inventory.vxi"),991)&&!savedInventory.count(Item::ResonantCore),"inventory_snapshot_matches_exploration_generation");
            const auto worldPath=g.worldSavePath("voxel_world.vxw");std::ifstream f(worldPath,std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(f)),{});f.close();
            {SaveFile::Checkpoint checkpoint(g.worldSavePath("save.checkpoint"));check(checkpoint.ready()&&g.world_->saveWorld(worldPath,g.player_->position()),"interrupted_generation_staged");}
            std::ifstream unchanged(worldPath,std::ios::binary);check(std::string((std::istreambuf_iterator<char>(unchanged)),{})==bytes&&SaveFile::verifyCheckpoint(g.activeWorld_.directory.string()),"abandoned_checkpoint_retains_verified_ERL1");unchanged.close();
            auto corrupt=bytes;const auto offset=corrupt.rfind("ERL1");check(offset!=std::string::npos,"ERL1_present");corrupt[offset+8]=char(0xff);
            const auto invalid=(out/"invalid-extension.vxw").string();{std::ofstream bad(invalid,std::ios::binary);bad.write(corrupt.data(),corrupt.size());}
            World untouched(991,23);untouched.adventure.biomes=4;glm::vec3 original(1,80,1);check(!untouched.loadWorld(invalid,original)&&untouched.adventure.biomes==4&&untouched.generationVersion()==23&&original==glm::vec3(1,80,1),"corrupted_extension_no_partial_mutation");
        }
    }
    static std::string signature(const World::GeneratedChunk& c){
        std::ostringstream out(std::ios::binary);out.write(reinterpret_cast<const char*>(c.blocks.data()),c.blocks.size()*sizeof(Block));
        for(const auto& m:c.loot){out.write(reinterpret_cast<const char*>(&m.x),4);out.write(reinterpret_cast<const char*>(&m.y),4);out.write(reinterpret_cast<const char*>(&m.z),4);out.put(char(m.table));out.write(reinterpret_cast<const char*>(&m.salt),4);}
        for(const auto& m:c.mobs){out.write(reinterpret_cast<const char*>(&m.x),4);out.write(reinterpret_cast<const char*>(&m.y),4);out.write(reinterpret_cast<const char*>(&m.z),4);out.put(char(m.hostile));}return out.str();
    }
    static void generation(const std::filesystem::path& out){
        std::ofstream sites(out/"native-sites.csv");sites<<"seed,kind,region_x,region_z,x,y,z,entrance_x,entrance_y,entrance_z\n";
        for(unsigned seed:{20260917U,123456789U,42U}){
            World w(seed,24),historical(seed,23);std::array<std::shared_ptr<const UndergroundPlan>,3> found{};
            for(int radius=0;radius<=16&&std::count_if(found.begin(),found.end(),[](auto p){return bool(p);})<3;++radius){
                for(int z=-radius;z<=radius;++z)for(int x=-radius;x<=radius;++x){if(radius&&std::max(std::abs(x),std::abs(z))!=radius)continue;
                    auto p=w.undergroundPlan(x,z);if(!p||p->kind==UndergroundKind::Mineshaft)continue;const auto c=relicChamber(*p,seed);if(!found[int(c.kind)])found[int(c.kind)]=p;}
            }
            std::vector<std::pair<int,int>> coordinates;
            for(int kind=0;kind<3;++kind){check(bool(found[kind]),"native_archetype_seed_"+std::to_string(seed)+"_"+std::to_string(kind));auto p=found[kind];auto c=relicChamber(*p,seed);
                sites<<seed<<','<<kind<<','<<p->regionX<<','<<p->regionZ<<','<<c.center.x<<','<<c.center.y<<','<<c.center.z<<','<<p->entrance.x<<','<<p->entrance.y<<','<<p->entrance.z<<'\n';sites.flush();
                const auto a=w.generationDebugAt(c.center.x,c.center.z),b=historical.generationDebugAt(c.center.x,c.center.z);
                check(a.terrainHeight==b.terrainHeight&&a.biome==b.biome&&a.temperature==b.temperature&&a.river==b.river,"V24_retains_V23_terrain_fields");
                std::set<std::tuple<int,int,int>> unique;for(auto cell:p->cells){check(cell.y>=-64&&cell.y<320&&validBlockState(cell.block),"native_cell_valid");check(unique.emplace(cell.x,cell.y,cell.z).second,"unique_plan_cell");}
                // Interior walking proof: controls reachable while sealed; treasure
                // approach reachable after opening. Neither relies on mining.
                std::map<std::tuple<int,int,int>,Block> blocks;for(auto cell:p->cells)blocks[{cell.x,cell.y,cell.z}]=cell.block;
                auto block=[&](int x,int y,int z){auto it=blocks.find({x,y,z});return it==blocks.end()?Block::Stone:it->second;};
                if(c.kind!=RelicPuzzle::Sequence){bool visibleBand=true;for(int x=-4;x<=4;++x)visibleBand=visibleBand&&block(c.center.x+x,c.center.y,c.center.z)==Block::TerracottaRed;check(visibleBand,"native_trial_has_visible_red_band");}
                auto reachable=[&](bool unlocked){std::set<std::pair<int,int>> visited;std::queue<std::pair<int,int>> queue;queue.push({0,0});visited.insert({0,0});
                    while(!queue.empty()){auto [x,z]=queue.front();queue.pop();for(auto d:{glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}){const int X=x+d.x,Z=z+d.y;if(std::abs(X)>6||std::abs(Z)>6||visited.count({X,Z}))continue;auto body=block(c.center.x+X,c.center.y+1,c.center.z+Z),head=block(c.center.x+X,c.center.y+2,c.center.z+Z);if(unlocked&&glm::ivec3(c.center.x+X,c.center.y+1,c.center.z+Z)==c.gate())body=Block::Air;if(!isSolid(body)&&!isSolid(head)){visited.insert({X,Z});queue.push({X,Z});}}}return visited;};
                auto closed=reachable(false),opened=reachable(true);for(int n:{0,2})check(closed.count({-3+n*3,-2})!=0,"control_approach_without_mining");check(opened.count({4,4})!=0,"unlocked_treasure_approach");
                for(int z=adventureFloorDiv(c.center.z-8,16);z<=adventureFloorDiv(c.center.z+8,16);++z)for(int x=adventureFloorDiv(c.center.x-8,16);x<=adventureFloorDiv(c.center.x+8,16);++x)coordinates.push_back({x,z});
            }
            std::vector<std::string> expected;for(auto [x,z]:coordinates)expected.push_back(signature(w.generateChunkData(x,z)));
            for(int workers:{1,2,4,8}){World parallel(seed,24);std::vector<std::string> actual(coordinates.size());std::vector<std::future<void>> jobs;
                for(int n=0;n<workers;++n)jobs.push_back(std::async(std::launch::async,[&,n]{for(std::size_t i=n;i<coordinates.size();i+=workers){const auto index=coordinates.size()-1-i;auto [x,z]=coordinates[index];actual[index]=signature(parallel.generateChunkData(x,z));}}));
                for(auto& job:jobs)job.get();check(actual==expected,"native_worker_order_"+std::to_string(workers));}
            if(Persistence::enabled()){
                const auto c=relicChamber(*found[0],seed);w.prepareSpawnTerrain(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));
                const auto path=(out/("native-"+std::to_string(seed)+".vxw")).string();check(w.saveWorld(path,glm::vec3(c.center)+glm::vec3(.5f,1,.5f)),"native_unmodified_devices_save");World loaded(seed,1);glm::vec3 pos;check(loaded.loadWorld(path,pos)&&loaded.generationVersion()==24,"native_unmodified_AUT1_reload");
            }
        }
        std::ofstream spawn(out/"spawns.csv");spawn<<"seed,x,y,z,safe\n";
        for(unsigned n=0;n<50;++n){World w(991+n*7919U,24);const auto p=w.findSurvivalSpawnNear(0,0);w.prepareSpawnTerrain(p);const auto cell=glm::ivec3(glm::floor(p));const bool safe=isSolid(w.getBlock(cell.x,cell.y-1,cell.z))&&!isSolid(w.getBlock(cell.x,cell.y,cell.z))&&!isSolid(w.getBlock(cell.x,cell.y+1,cell.z));check(safe,"V24_spawn_"+std::to_string(n));spawn<<w.seed()<<','<<p.x<<','<<p.y<<','<<p.z<<','<<safe<<'\n';}
    }
    static void ui(Game& g,const std::filesystem::path& out,const std::string& worldFile){
        World loaded(20260917,24);glm::vec3 pos;check(loaded.loadWorld(worldFile,pos)&&loaded.adventure.biomes!=0,"UI_uses_recorded_native_journal_state");
        std::ofstream sizes(out/"ui-targets.csv");sizes<<"width,height,scale,category,gl_error\n";
        for(auto size:{glm::ivec2(1280,720),glm::ivec2(1920,1080),glm::ivec2(2560,1440)}){
            GLuint fb=0,texture=0;glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size.x,size.y,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
            check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"native_UI_framebuffer_complete");glViewport(0,0,size.x,size.y);
            for(int scale=0;scale<4;++scale)for(int category=0;category<4;++category){
                const auto factor=UiLayout::interfaceScale(size.x,size.y,scale);glClearColor(.10f,.13f,.16f,1);glClear(GL_COLOR_BUFFER_BIT);
                g.renderer_->renderAdventure(int(size.x/factor),int(size.y/factor),loaded.adventure,category,0);
                const auto error=glGetError();sizes<<size.x<<','<<size.y<<','<<scale<<','<<category<<','<<error<<'\n';check(error==GL_NO_ERROR,"native_UI_scale_category_GL_valid");
                const auto dir=out/("ui-"+std::to_string(size.x)+"-s"+std::to_string(scale)+"-c"+std::to_string(category));check(Screenshot::saveBmp(size.x,size.y,dir.string()),"exact_native_UI_capture");
            }
            glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fb);
        }
    }
    static void route(Game& g,const std::filesystem::path& out,int fps,int rd){
        g.settings_.renderDistance=rd;g.settings_.simulationDistance=8;g.settings_.vsync=false;g.settings_.musicVolume=0;g.settings_.frameLimit=0;
        const auto saved=WorldLibrary::create("Native relic route "+std::to_string(fps),20260917,GameMode::Survival,false);g.playSavedWorld(saved);check(g.world_&&g.world_->generationVersion()==24,"route_new_V24_world");
        auto& w=*g.world_;auto& inv=*g.inventory_;g.showDebug_=false;g.fullbright_=false;glfwShowWindow(g.window_);glfwFocusWindow(g.window_);glfwSwapInterval(0);
        auto aim=[&](glm::vec3 target){const auto d=glm::normalize(target-g.player_->cameraPosition());g.player_->yaw_=glm::degrees(std::atan2(d.z,d.x));g.player_->pitch_=glm::degrees(std::asin(d.y));};
        std::string phase="walking";std::map<std::string,std::vector<double>> frameTimes,workTimes,gpuTimes;std::map<std::string,double> peakRam,peakTerrain;
        struct Query{GLuint id=0;bool pending=false;std::string phase;};std::array<Query,4> queries{};GLint timerBits=0;glGetQueryiv(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&timerBits);
        if(timerBits)for(auto& q:queries)glGenQueries(1,&q.id);
        // Match the game's high-resolution limiter. sleep_until alone can
        // oversleep by a Windows timer quantum and mislabel 60 FPS as ~33 FPS.
#ifdef _WIN32
        struct FrameTimer { HANDLE handle=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);~FrameTimer(){if(handle)CloseHandle(handle);} } limiter;
#endif
        auto waitUntil=[&](std::chrono::steady_clock::time_point deadline){
            const double remaining=std::chrono::duration<double>(deadline-std::chrono::steady_clock::now()).count();if(remaining<=0)return;
#ifdef _WIN32
            if(limiter.handle){LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(remaining*10000000));if(SetWaitableTimer(limiter.handle,&due,0,nullptr,nullptr,FALSE)&&WaitForSingleObject(limiter.handle,1000)==WAIT_OBJECT_0)return;}
#endif
            std::this_thread::sleep_until(deadline);
        };
        auto frame=[&](bool render=true){const auto start=std::chrono::steady_clock::now();glfwPollEvents();g.timing_.previousFrame=glfwGetTime();g.sounds_->update(1.f/fps);g.updateSimulation(1.f/fps);
            Query* active=nullptr;for(auto& q:queries)if(q.id){if(q.pending){GLuint ready=0;glGetQueryObjectuiv(q.id,GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 value=0;glGetQueryObjectui64v(q.id,GL_QUERY_RESULT,&value);gpuTimes[q.phase].push_back(value/1000000.);q.pending=false;}}if(!q.pending&&!active)active=&q;}
            if(active&&render)glBeginQuery(GL_TIME_ELAPSED,active->id);if(render)g.renderFrame(1.f/fps);if(active&&render){glEndQuery(GL_TIME_ELAPSED);active->pending=true;active->phase=phase;}
            if(frameTimes[phase].size()%15==0){peakRam[phase]=std::max(peakRam[phase],fixtureWorkingSetMb());peakTerrain[phase]=std::max(peakTerrain[phase],double(w.terrainMemory().capacityBytes));}
            const auto done=std::chrono::steady_clock::now();workTimes[phase].push_back(std::chrono::duration<double,std::milli>(done-start).count());
            waitUntil(start+std::chrono::microseconds(1000000/fps));frameTimes[phase].push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());check(!g.player_->isDead(),"route_player_alive");};

        auto settle=[&]{for(int i=0;i<120;++i){w.setStreamingView(g.player_->lookDirection(),5);w.updateStreaming(g.player_->cameraPosition(),8);g.updateTargets();g.renderFrame(0);glfwPollEvents();}};
        auto capture=[&](std::string name){g.screenshotRequested_=true;g.renderFrame(0);std::filesystem::path last;for(auto f:std::filesystem::directory_iterator("screenshots"))if(last.empty()||f.last_write_time()>std::filesystem::last_write_time(last))last=f.path();check(!last.empty(),"native_capture");std::filesystem::copy_file(last,out/(std::to_string(fps)+"-"+name+".bmp"),std::filesystem::copy_options::overwrite_existing);};
        auto select=[&](Item item){for(int i=0;i<36;++i)if(inv.slots_[i].item==item){if(i>=9)std::swap(inv.slots_[i],inv.slots_[8]);inv.selectSlot(i>=9?8:i);return;}throw std::runtime_error("Route item absent "+std::to_string(int(item)));};
        auto use=[&](glm::vec3 target){aim(target);g.updateTargets();g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.input_.previousMouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=false;g.updateEquipmentInput(true);g.handleUseAction();g.input_.mouseButtons_.fill(false);g.player_->setBlocking(false);};
        // Reproducible established-Survival starting kit. Relics and core must
        // come from native, finite generated containers, never fixture grants.
        inv.clear();for(auto entry:{std::pair<Item,int>{Item::DiamondSword,1},{Item::IronPickaxe,1},{Item::IronIngot,16},{Item::CopperIngot,8},{Item::GoldIngot,6},{Item::Glass,4},{Item::Compass,1},{Item::CraftingTable,1},{Item::Diamond,2},{Item::Bread,16}})check(inv.add(entry.first,entry.second)==0,"route_common_starting_kit");
        for(auto item:{Item::IronHelmet,Item::IronChestplate,Item::IronLeggings,Item::IronBoots}){check(inv.add(item)==0,"route_prior_armor");for(int i=0;i<36;++i)if(inv.slots_[i].item==item){check(inv.equipFromSlot(i),"route_equip");break;}}
        // Discover a genuine surface landmark with proximity, then enter a real
        // native dungeon. Long route segments use recorded fixture teleports.
        bool landmark=false;
        for(int radius=0;radius<12&&!landmark;++radius)for(int z=-radius;z<=radius&&!landmark;++z)for(int x=-radius;x<=radius&&!landmark;++x){if(radius&&std::max(std::abs(x),std::abs(z))!=radius)continue;auto p=w.structures_.smallPlanForRegion(x,z,[&](int X,int Z){return w.structureTerrainAt(X,Z);});if(p.pieces.empty())continue;const auto piece=p.pieces.front();auto spawn=w.findSafeSpawnNear(piece.x,piece.z+4);w.prepareSpawnTerrain(spawn);g.player_->teleport(spawn);aim(glm::vec3(piece.x,piece.y+2,piece.z));settle();for(int i=0;i<15;++i)frame();if(!w.adventure.discoveries().empty()){capture("landmark");landmark=true;}}
        check(landmark,"route_native_landmark_discovery");
        auto plan=w.undergroundPlan(0,2);check(plan&&plan->kind!=UndergroundKind::Mineshaft,"route_native_dungeon");const auto c=relicChamber(*plan,w.seed());check(c.kind==RelicPuzzle::Sequence,"route_sequence_fixture");
        g.player_->teleport(glm::vec3(plan->entrance.x+.5f,plan->entrance.y,plan->entrance.z+.5f));w.prepareSpawnTerrain(g.player_->position());aim(glm::vec3(plan->rooms.front().x,plan->rooms.front().y+2,plan->rooms.front().z));settle();capture("cave-approach");
        auto supply=std::find_if(plan->loot.begin(),plan->loot.end(),[](auto m){return m.table==StructureLoot::RelicSupplies;});check(supply!=plan->loot.end(),"route_native_relic_cache");
        g.player_->teleport(glm::vec3(supply->x+1.5f,supply->y,supply->z+.5f));w.prepareSpawnTerrain(g.player_->position());settle();use(glm::vec3(supply->x+.5f,supply->y+.5f,supply->z+.5f));check(g.ui_.state()==GameState::Chest,"route_actual_RMB_cache");capture("relic-cache");auto chest=w.chestAt({supply->x,supply->y,supply->z});check(chest&&inv.transferContainer(chest->slots.data(),27,2)>0,"route_take_native_relics");check(inv.count(Item::SurveyorToken)&&inv.count(Item::CavernShard)&&inv.count(Item::AncientSeal),"route_three_native_relics");g.ui_.closeGameplayInterface(inv);g.synchronizeCursorCapture();
        g.player_->teleport(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));w.prepareSpawnTerrain(g.player_->position());aim(glm::vec3(c.center)+glm::vec3(0,2,-4));settle();capture("chamber-unsolved");
        select(Item::CraftingTable);use(glm::vec3(c.center)+glm::vec3(-2.5f,.01f,.5f));
        glm::ivec3 bench(0);bool placedBench=false;for(int z=-6;z<=6;++z)for(int x=-6;x<=6;++x){auto q=c.center+glm::ivec3(x,1,z);if(w.getBlock(q.x,q.y,q.z)==Block::CraftingTable){bench=q;placedBench=true;}}
        check(placedBench&&!inv.count(Item::CraftingTable),"route_actual_workbench_placement");

        auto craft=[&](Item item){use(glm::vec3(bench)+glm::vec3(.5f));check(g.ui_.state()==GameState::CraftingTable,"route_open_placed_bench");int recipe=-1;for(int i=0;i<int(craftingRecipes().size());++i)if(craftingRecipes()[i].output==item)recipe=i;check(recipe>=0&&inv.fillRecipe(recipe,true,false)&&inv.craftOutputToInventory(true)==1,"route_production_book_craft_"+std::to_string(int(item)));g.ui_.closeGameplayInterface(inv);g.synchronizeCursorCapture();};
        craft(Item::SurveyorCompass);craft(Item::EchoLantern);select(Item::SurveyorCompass);use(glm::vec3(c.center)+glm::vec3(0,0,-2));check(!g.transportation_->waypoints().empty(),"route_surveyor_navigation");
        select(Item::EchoLantern);use(glm::vec3(c.center)+glm::vec3(1.5f,.01f,-1.5f));check(inv.selectedStack().durability==63,"route_lantern_charge");capture("relic-tools");
        select(Item::DiamondSword);for(int n=0;n<3;++n)use(glm::vec3(c.control(n))+glm::vec3(.5f,.25f,.5f));
        phase="ambush";for(int n=0;n<fps*20&&!(!w.adventure.locked(c));++n){
            bool aimAtEnemy=false;for(const auto& a:g.survival_->animals_)if(a.health>0&&a.deathTimer<=0&&a.relicEncounter){aim(a.position+glm::vec3(0,1,0));aimAtEnemy=true;break;}
            g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_LEFT]=aimAtEnemy;frame();if(n==fps)capture("finite-ambush");
        }
        g.input_.mouseButtons_.fill(false);check(!w.adventure.locked(c),"route_real_combat_trial_won");
        // Walk on native room geometry through the now-open gate.
        auto walk=[&](glm::vec3 target){phase="walking";for(int n=0;n<fps*6&&glm::length(glm::vec2(target.x-g.player_->position().x,target.z-g.player_->position().z))>.3f;++n){aim({target.x,g.player_->cameraPosition().y,target.z});g.input_.keys_[GLFW_KEY_W]=true;frame();}g.input_.keys_.fill(false);check(glm::length(glm::vec2(target.x-g.player_->position().x,target.z-g.player_->position().z))<.4f,"route_native_walk");};
        walk(glm::vec3(c.center)+glm::vec3(1.5f,1,4.5f));walk(glm::vec3(c.center)+glm::vec3(3.5f,1,4.5f));use(glm::vec3(c.chest())+glm::vec3(.5f));check(g.ui_.state()==GameState::Chest,"route_unlocked_treasure_input");capture("treasure");auto vault=w.chestAt(c.chest());check(vault&&inv.transferContainer(vault->slots.data(),27,2)>0&&inv.count(Item::ResonantCore),"route_finite_core_reward");g.ui_.closeGameplayInterface(inv);g.synchronizeCursorCapture();walk(glm::vec3(c.center)+glm::vec3(1.5f,1,4.5f));walk(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));craft(Item::AncientExplorerTool);check(inv.count(Item::AncientExplorerTool),"route_rare_upgrade");
        if(fps==30){
            for(auto region:{glm::ivec2(0,-1),glm::ivec2(-1,0)}){
                auto other=w.undergroundPlan(region.x,region.y);check(other&&other->kind!=UndergroundKind::Mineshaft,"gallery_native_plan");const auto chamber=relicChamber(*other,w.seed());
                g.player_->teleport(glm::vec3(chamber.center)+glm::vec3(.5f,1,-1.5f));w.prepareSpawnTerrain(g.player_->position());
                const auto interactionPose=g.player_->position();
                // Frame both inputs and the marked floor, rather than standing
                // so close that the warning pillar hides the circuit.
                g.player_->teleport(glm::vec3(chamber.center)+glm::vec3(.5f,1,3.5f));
                aim(glm::vec3(chamber.center)+glm::vec3(.5f,1.5f,-3));settle();capture("puzzle-"+std::to_string(int(chamber.kind)));
                g.player_->teleport(interactionPose);
                if(chamber.kind==RelicPuzzle::Routing){for(int n:{0,2})use(glm::vec3(chamber.control(n))+glm::vec3(.5f,.25f,.5f));}
                else {for(int n:{0,2}){walk(glm::vec3(chamber.control(n))+glm::vec3(.5f,.02f,.5f));for(int i=0;i<5;++i)frame();}walk(glm::vec3(chamber.center)+glm::vec3(.5f,1,-1.5f));}
                phase="pulse-trial";const auto health=g.player_->health();bool warningCaptured=false;int litFrames=0;
                for(int i=0;i<fps*14&&w.adventure.locked(chamber);++i){frame();const auto lamp=chamber.center+glm::ivec3(0,3,-5);if(blockLightEmission(w.getBlock(lamp.x,lamp.y,lamp.z))>0)++litFrames;else litFrames=0;if(!warningCaptured&&litFrames>=4){aim(glm::vec3(lamp)+glm::vec3(.5f));g.updateTargets();capture("pulse-warning-"+std::to_string(int(chamber.kind)));warningCaptured=true;}}
                check(warningCaptured&&!w.adventure.locked(chamber)&&g.player_->health()>=health,"native_visible_trial_safe_route");
            }
        }
        g.input_.keys_[GLFW_KEY_J]=true;g.input_.previousKeys_[GLFW_KEY_J]=false;g.adventureInput();check(g.journalOpen_,"route_configurable_journal_input");g.input_.keys_.fill(false);
        for(auto resolution:{glm::ivec2(1280,720),glm::ivec2(1920,1080),glm::ivec2(2560,1440)}){glfwSetWindowSize(g.window_,resolution.x,resolution.y);glfwPollEvents();capture("journal-"+std::to_string(resolution.x));}g.journalOpen_=false;g.synchronizeCursorCapture();
        if(Persistence::enabled()){
            // A request coalesced behind an older autosave is not a fresh capture.
            // Drain it explicitly, retain that wait, then time a known new snapshot.
            const auto drainStart=std::chrono::steady_clock::now();g.pollSave(true);
            const auto saveStart=std::chrono::steady_clock::now();check(g.saveAll(true),"route_async_save");
            const auto captured=std::chrono::steady_clock::now();const auto status=g.pendingSaveStatus_;
            check(status&&status->snapshotBytes>0,"route_measures_real_immutable_capture");
            g.pollSave(true);const auto committed=std::chrono::steady_clock::now();
            std::ofstream(out/"checkpoint-ms.csv")<<"prior_pending_wait_ms,request_ms,capture_ms,total_ms,snapshot_bytes\n"
                <<std::chrono::duration<double,std::milli>(saveStart-drainStart).count()<<','
                <<std::chrono::duration<double,std::milli>(captured-saveStart).count()<<','<<status->captureMilliseconds<<','
                <<std::chrono::duration<double,std::milli>(committed-saveStart).count()<<','<<status->snapshotBytes<<'\n';
            check(SaveFile::verifyCheckpoint(saved.directory.string()),"route_checkpoint_verified");g.returnToMainMenu();g.playSavedWorld(saved);
            check(g.world_&&!g.world_->adventure.locked(c)&&g.inventory_->count(Item::AncientExplorerTool)&&g.world_->chestAt(c.chest(),false)!=nullptr,"route_reload_generation_and_ownership");
            g.world_->prepareSpawnTerrain(glm::vec3(c.center)+glm::vec3(.5f,1,.5f));check(g.world_->chestAt(c.chest())&&g.world_->chestAt(c.chest())->slots[0].empty(),"route_no_reward_reroll");
        }
        auto mean=[](const std::vector<double>& v){double n=0;for(auto x:v)n+=x;return v.empty()?0:n/v.size();};
        auto percentile=[](std::vector<double> v,double p){if(v.empty())return 0.;std::sort(v.begin(),v.end());return v[std::size_t((v.size()-1)*p)];};
        std::ofstream performance(out/"native-performance.csv");performance<<"rd,target_fps,phase,samples,mean_frame_ms,p95_frame_ms,p99_frame_ms,worst_frame_ms,mean_work_ms,p95_work_ms,p99_work_ms,worst_work_ms,gpu_samples,mean_gpu_ms,worst_gpu_ms,ram_mb,terrain_capacity,puzzles\n";
        for(const auto& [name,times]:frameTimes){const auto& work=workTimes[name];const auto& gpu=gpuTimes[name];performance<<rd<<','<<fps<<','<<name<<','<<times.size()<<','<<mean(times)<<','<<percentile(times,.95)<<','<<percentile(times,.99)<<','<<percentile(times,1)<<','<<mean(work)<<','<<percentile(work,.95)<<','<<percentile(work,.99)<<','<<percentile(work,1)<<','<<gpu.size()<<','<<mean(gpu)<<','<<percentile(gpu,1)<<','<<peakRam[name]<<','<<peakTerrain[name]<<','<<g.world_->adventure.puzzles().size()<<'\n';}
        for(auto& q:queries)if(q.id)glDeleteQueries(1,&q.id);g.saveOnExit_=false;
    }
public:
    static int run(int argc,char** argv){
        const auto out=std::filesystem::absolute(argc>2?argv[2]:"adventure-evidence");std::filesystem::create_directories(out);
        Game g;char app[]="AdventureHarness";char* args[]={app};check(g.initialize(1,args),"native_initialize");glfwHideWindow(g.window_);
        if(std::string(argv[1])=="--adventure-review")review();else if(std::string(argv[1])=="--adventure-generation")generation(out);else if(std::string(argv[1])=="--adventure-ui"){if(argc<6)throw std::runtime_error("UI fixture needs an isolated native world copy");ui(g,out,argv[5]);}else if(std::string(argv[1])=="--adventure-route")route(g,out,argc>3?std::stoi(argv[3]):30,argc>4?std::stoi(argv[4]):8);else {conservation(out);chambers(g,out);}g.saveOnExit_=false;g.shutdown();
        std::ofstream(out/"results.json")<<"{\"assertions\":"<<checks<<",\"passed\":true}\n";
        std::cout<<"Adventure checks passed: "<<checks<<std::endl;return 0;
    }
};
int runAdventureTests(int argc,char** argv){return AdventureTest::run(argc,argv);}
