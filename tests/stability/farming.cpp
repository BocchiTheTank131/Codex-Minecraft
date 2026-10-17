#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Game.h"
#include "Player.h"
#include "Farming.h"
#include "FarmingBaseline.h"
#include "Definitions.h"
#include "Renderer.h"
#include "Weather.h"
#include "Sound.h"
#include "Transportation.h"
#include "SaveFile.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

class FarmingTest {
    static inline int assertions=0;
    static void check(bool ok,const std::string& name) {
        std::cout<<"farming "<<name<<" passed="<<ok<<std::endl;
        ++assertions;if(!ok)throw std::runtime_error(name);
    }
    static std::string bytes(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
    static void write(const std::filesystem::path& p,const std::string& s) {std::ofstream f(p,std::ios::binary);f.write(s.data(),s.size());}
public:
    static int run(int argc,char** argv);
    static int showcase(Game&,const std::filesystem::path&);
    static int stress(Game&,const std::filesystem::path&);
};
bool testFarmingControls(Game&);
int runFarmingTests(int argc,char** argv) {return FarmingTest::run(argc,argv);}

int FarmingTest::run(int argc,char** argv) {
    Game g;char app[]="farming-regression";char* args[]={app};check(g.initialize(1,args),"game_initialize");
    const bool visual=argc>1 && std::string(argv[1])=="--farming-visual";
    if(!visual)glfwHideWindow(g.window_);
    const auto selected=WorldLibrary::create("Farming isolated fixture",991,GameMode::Survival,true);
    g.playSavedWorld(selected);check(bool(g.world_),"world_open");
    if(visual)return showcase(g,argc>2?std::filesystem::path(argv[2]):std::filesystem::current_path());
    if(argc>1 && std::string(argv[1])=="--farming-benchmark")return stress(g,argc>2?std::filesystem::path(argv[2]):std::filesystem::current_path());
    auto& w=*g.world_;auto& inventory=*g.inventory_;auto& animals=*g.survival_;
    const auto originalBillboards=animals.renderBillboards();
    for(int x:{-12,0,24})for(int z:{-12,0,18})w.prepareSpawnTerrain({float(x),300,float(z)});
    w.setSimulationDistance(7);w.setSimulationDistance(8);
    w.fillBlocks({-12,299,-12},{24,299,18},Block::Dirt);
    w.fillBlocks({-12,300,-12},{24,304,18},Block::Air);
    g.player_->teleport({.5f,300,6.5f});
    FarmingSystem farm;
    const Item seeds[]={Item::Seeds,Item::Carrot,Item::Potato,Item::BeetrootSeeds,Item::PumpkinSeeds,Item::MelonSeeds};
    for(int i=0;i<6;++i) {
        const glm::ivec3 p(-8+i*3,300,-4);w.setBlock(p.x,299,p.z,Block::WetFarmland);
        check(farm.plant(w,p,seeds[i]),"plant_"+std::to_string(i));
        check(!farm.plant(w,p,seeds[i]),"occupied_"+std::to_string(i));
        for(int j=0;j<3 && cropStage(w.getBlock(p.x,p.y,p.z))<3;++j)check(farm.fertilize(w,p),"fertilize_"+std::to_string(i));
        check(cropStage(w.getBlock(p.x,p.y,p.z))==3,"mature_"+std::to_string(i));
        auto drops=FarmingSystem::harvest(w.getBlock(p.x,p.y,p.z),p,991);
        check(!drops.empty(),"harvest_"+std::to_string(i));
        if(i>=4) {
            check(!farm.fertilize(w,p),"mature_stem_fertilizer_loop_refused");
            for(int tick=0;tick<8400;++tick)farm.update(1.f/120,w,g.player_->position());
            bool found=false;for(auto d:{glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1)}) {
                const auto q=p+d;if(w.getBlock(q.x,q.y,q.z)==(i==4?Block::Pumpkin:Block::Melon)){w.setBlock(q.x,q.y,q.z,Block::Air);found=true;}
            }
            check(found,"stem_produces_natural_fruit");
            for(int tick=0;tick<8400;++tick)farm.update(1.f/120,w,g.player_->position());
            int fruits=0;for(auto d:{glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1)})if(w.getBlock(p.x+d.x,p.y,p.z+d.z)==(i==4?Block::Pumpkin:Block::Melon))++fruits;
            check(fruits==1,"stem_repeats_once_after_harvest");
        }
    }
    const glm::ivec3 blocked(14,300,-4);w.setBlock(14,299,-4,Block::WetFarmland);w.setBlock(14,300,-4,Block::PumpkinStem3);
    for(auto d:{glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1)})w.setBlock(14+d.x,300,-4+d.z,Block::Stone);
    check(!farm.fertilize(w,blocked),"blocked_stem_no_overwrite");
    check(!farm.plant(w,{2,301,2},Item::Carrot),"no_plant_in_air");
    check(FarmingSystem::harvest(Block::Farmland,{0,0,0},991).front().item==Item::Dirt && FarmingSystem::harvest(Block::WetFarmland,{0,0,0},991).front().item==Item::Dirt,"both_farmland_states_drop_dirt");
    for(int x:{0,3}) {w.setBlock(x,299,9,Block::Farmland);w.setBlock(x,300,9,Block::Crop0);}
    w.fillBlocks({1,301,6},{6,301,12},Block::Stone);
    check(w.sunlightAt(0,300,9)>=9 && w.sunlightAt(3,300,9)<9,"growth_light_fixture");
    for(int i=0;i<21600;++i)farm.update(1.f/120,w,g.player_->position());
    check(w.getBlock(0,300,9)==Block::Crop3 && w.getBlock(3,300,9)==Block::Crop0,"natural_growth_and_darkness");
    w.setBlock(3,299,9,Block::Stone);
    for(int i=0;i<600;++i)farm.update(1.f/120,w,g.player_->position());
    check(w.getBlock(3,300,9)==Block::Air,"removed_soil_clears_plant");
    const glm::ivec3 wet(0,299,2);w.setBlock(4,299,2,Block::Water);
    check(FarmingSystem::hydrated(w,wet),"water_distance_four");w.setBlock(4,299,2,Block::Air);
    check(!FarmingSystem::hydrated(w,wet),"water_removal");w.setBlock(4,299,2,Block::WaterFlow3);
    check(FarmingSystem::hydrated(w,wet),"flowing_irrigation");
    w.setBlock(0,299,2,Block::Farmland);
    for(int i=0;i<600;++i)farm.update(1.f/120,w,g.player_->position());
    check(w.getBlock(0,299,2)==Block::WetFarmland,"soil_hydrates");
    w.setBlock(4,299,2,Block::Air);for(int i=0;i<600;++i)farm.update(1.f/120,w,g.player_->position());
    check(w.getBlock(0,299,2)==Block::Farmland,"soil_dries");
    const glm::ivec3 cane(1,300,3);w.setBlock(1,299,3,Block::Sand);w.setBlock(2,299,3,Block::Water);w.setBlock(1,300,3,Block::SugarCane);
    check(farm.fertilize(w,cane) && farm.fertilize(w,cane+glm::ivec3(0,1,0)),"cane_grows_up");
    check(!farm.fertilize(w,cane+glm::ivec3(0,2,0)) && w.getBlock(1,303,3)==Block::Air,"cane_height_three");
    const glm::ivec3 berry(4,300,4);check(farm.plant(w,berry,Item::SweetBerries),"berry_plant");
    for(int i=0;i<3 && w.getBlock(4,300,4)!=Block::BerryBush3;++i)check(farm.fertilize(w,berry),"berry_growth");
    check(farm.interact(w,inventory,animals,berry,berry+glm::ivec3(0,1,0),{0,1,0},Item::None,false,false) && w.getBlock(4,300,4)==Block::BerryBush1,"berry_repeated_harvest");
    for(int i=0;i<5;++i) {
        const auto hoe=Item(int(Item::WoodHoe)+i);inventory.clear();inventory.add(hoe);const int durability=inventory.selectedStack().durability;
        const glm::ivec3 soil(15+i,299,5);w.setBlock(soil.x,soil.y,soil.z,Block::Dirt);
        check(farm.interact(w,inventory,animals,soil,soil+glm::ivec3(0,1,0),{0,1,0},hoe,false,false) && isFarmland(w.getBlock(soil.x,soil.y,soil.z)) && inventory.selectedStack().durability==durability-1,"hoe_tier_"+std::to_string(i));
        w.setBlock(soil.x,299,6,Block::Dirt);w.setBlock(soil.x,300,6,Block::Stone);
        farm.interact(w,inventory,animals,{soil.x,299,6},{soil.x,300,6},{0,1,0},hoe,false,false);
        check(w.getBlock(soil.x,299,6)==Block::Dirt && inventory.selectedStack().durability==durability-1,"obstructed_till_"+std::to_string(i));
    }
    inventory.clear();inventory.add(Item::Wheat,8);w.setBlock(8,300,6,Block::Composter0);
    for(int i=0;i<4;++i)check(farm.interact(w,inventory,animals,{8,300,6},{8,301,6},{0,1,0},Item::Wheat,false,false),"compost_feed");
    check(inventory.count(Item::Wheat)==4 && w.getBlock(8,300,6)==Block::Composter4,"compost_fill_consumption");
    farm.interact(w,inventory,animals,{8,300,6},{8,301,6},{0,1,0},Item::Wheat,false,false);
    check(w.getBlock(8,300,6)==Block::Composter0 && inventory.count(Item::Wheat)==4,"compost_output_no_extra_consumption");
    check(!animals.drops_.empty() && animals.drops_.back().stack.item==Item::BoneMeal && animals.drops_.back().stack.count==1,"compost_actual_fertilizer_output");
    inventory.clear();inventory.add(Item::BoneMeal,2);
    farm.interact(w,inventory,animals,blocked,blocked+glm::ivec3(0,1,0),{0,1,0},Item::BoneMeal,false,false);
    check(inventory.count(Item::BoneMeal)==2,"failed_fertilizer_not_consumed");
    bool acquired[8]{};
    const Item required[]={Item::Seeds,Item::Carrot,Item::Potato,Item::BeetrootSeeds,Item::PumpkinSeeds,Item::MelonSeeds,Item::SweetBerries,Item::SugarCane};
    for(int x=-3000;x<3000;++x)for(const auto& s:FarmingSystem::harvest(Block::TallGrass,{x,70,-81},991))for(int i=0;i<8;++i)if(s.item==required[i])acquired[i]=true;
    check(std::all_of(std::begin(acquired),std::end(acquired),[](bool b){return b;}),"legacy_world_acquisition_all_seeds");
    const glm::ivec3 caveCrop(10,-32,10);
    w.fillBlocks({7,-33,7},{13,-29,13},Block::Stone);
    w.fillBlocks({8,-32,8},{12,-30,12},Block::Air);
    w.setBlock(10,-33,10,Block::Farmland);w.setBlock(10,-32,10,Block::Crop0);w.setBlock(11,-32,10,Block::Torch);
    for(int i=0;i<40 && w.blockLightAt(10,-32,10)<9;++i)w.updateStreaming({10,300,10},0);
    check(w.blockLightAt(10,-32,10)>=9 && w.sunlightAt(10,-32,10)==0,"negative_y_torch_light_fixture");
    w.setBlock(0,300,9,Block::Crop0);
    for(int i=0;i<21600;++i)farm.update(1.f/120,w,g.player_->position(),nullptr,0);
    check(w.getBlock(10,-32,10)==Block::Crop3 && w.getBlock(0,300,9)==Block::Crop0,"underground_light_and_night_growth");
    // Negative Y, chunk border, far coordinates and no unloaded writes.
    for(const glm::ivec3& p:{glm::ivec3(-1,-32,-1),glm::ivec3(15,300,1),glm::ivec3(16000001,-32,-16000001)}) {
        w.prepareSpawnTerrain(glm::vec3(p));w.updateStreaming(glm::vec3(p),0);w.setBlock(p.x,p.y-1,p.z,Block::Farmland);w.setBlock(p.x,p.y,p.z,Block::Air);
        check(farm.plant(w,p,Item::Carrot) && farm.fertilize(w,p),"coordinate_crop_"+std::to_string(p.x));
    }
    w.prepareSpawnTerrain(g.player_->position());w.updateStreaming(g.player_->position(),0);
    check(!farm.plant(w,{29000000,100,29000000},Item::Potato),"unloaded_growth_refused");
    inventory.clear();inventory.add(Item::Carrot,2);inventory.swapOffhand();
    w.setBlock(12,299,10,Block::Farmland);w.setBlock(12,300,10,Block::Air);
    check(farm.interact(w,inventory,animals,{12,299,10},{12,300,10},{0,1,0},Item::Carrot,true,false) && inventory.equipment(4).count==1 && w.getBlock(12,300,10)==Block::Carrot0,"offhand_plant_consumes_owned_stack");
    g.player_->hunger_=10;g.player_->saturation_=0;
    check(consumeFood(*g.player_,inventory,true,false) && inventory.equipment(4).empty() && g.player_->hunger()==13,"offhand_food_consumption");
    check(testFarmingControls(g),"production_game_right_click_farming_food_shield_priority");
    // Player statistics and container replacement use production helpers.
    auto& player=*g.player_;player.hunger_=8;player.saturation_=0;player.health_=11;inventory.clear();inventory.add(Item::MushroomStew);
    for(int i=1;i<36;++i)inventory.add(Item::Stone,64);
    check(consumeFood(player,inventory,false,false) && inventory.selectedItem()==Item::Bowl && player.hunger()==14 && player.saturation()==7.2f,"full_inventory_meal_returns_bowl");
    player.hunger_=20;player.saturation_=5;inventory.clear();inventory.add(Item::Carrot,2);
    check(!consumeFood(player,inventory,false,false) && inventory.count(Item::Carrot)==2,"full_hunger_no_consumption");
    player.hunger_=10;
    check(consumeFood(player,inventory,false,true) && inventory.count(Item::Carrot)==2,"creative_food_owned_stack_unchanged");
    for(int id=174;id<int(Item::Count);++id)if(isFood(Item(id)))for(bool offhand:{false,true}) {
        const auto item=Item(id);const auto& definition=itemDefinition(item);
        player.hunger_=0;player.saturation_=0;inventory.clear();inventory.add(item);if(offhand)inventory.swapOffhand();
        check(consumeFood(player,inventory,offhand,false) && player.hunger()==definition.foodValue && player.saturation()==std::min(definition.foodValue,definition.saturation),"nutrition_consumption_"+std::to_string(id)+(offhand?"_offhand":"_main"));
        const auto result=offhand?inventory.equipment(4):inventory.selectedStack();
        check(definition.foodContainer==Item::None?result.empty():result.item==definition.foodContainer && result.count==1,"food_container_"+std::to_string(id)+(offhand?"_offhand":"_main"));
    }
    player.hunger_=12;player.saturation_=3;player.health_=9;
    player.setCreativeMode(true);player.setSpectatorMode(true);player.setSpectatorMode(false);player.setCreativeMode(false);
    check(player.health()==9 && player.hunger()==12 && player.saturation()==3,"gamemode_preserves_survival_stats");
    check(player.save("nutrition.vps",991),"nutrition_save");Player loadedPlayer({0,300,0});check(loadedPlayer.load("nutrition.vps",991) && loadedPlayer.saturation()==3 && loadedPlayer.health()==9 && loadedPlayer.hunger()==12,"nutrition_roundtrip");
    const auto nutrition=bytes("nutrition.vps");write("old.vps",nutrition.substr(0,24));check(loadedPlayer.load("old.vps",991) && loadedPlayer.saturation()==5,"old_player_migration");
    auto invalid=nutrition;const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(invalid.data()+28,&nan,4);write("invalid.vps",invalid);
    check(!loadedPlayer.load("invalid.vps",991) && loadedPlayer.saturation()==5,"invalid_nutrition_no_partial_load");
    // Animal resources, breeding, persistence and bounded egg production.
    animals.animals_.clear();animals.drops_.clear();w.prepareSpawnTerrain({0,300,0});
    for(auto type:{SurvivalWorld::AnimalType::Cow,SurvivalWorld::AnimalType::Sheep,SurvivalWorld::AnimalType::Chicken}) {
        SurvivalWorld::Animal a;a.type=type;a.position={float(animals.animals_.size()*3),300,0};a.home=a.position;a.health=type==SurvivalWorld::AnimalType::Chicken?4:10;animals.animals_.push_back(a);
    }
    inventory.clear();inventory.add(Item::Shears);
    check(animals.tendAnimal(1,inventory,false,false) && animals.animals_[1].sheared && inventory.selectedStack().durability==237,"shearing_once");
    check(!animals.tendAnimal(1,inventory,false,false) && inventory.selectedStack().durability==237,"shearing_no_repeat");
    inventory.clear();inventory.add(Item::Bucket);
    check(animals.tendAnimal(0,inventory,false,false) && inventory.count(Item::MilkBucket)==1 && inventory.count(Item::Bucket)==0,"milking_atomic");
    inventory.clear();inventory.add(Item::Bucket);check(!animals.tendAnimal(0,inventory,false,false) && inventory.count(Item::Bucket)==1,"milking_cooldown");
    animals.animals_[0].resourceTimer=0;inventory.clear();inventory.add(Item::Bucket,2);for(int i=1;i<36;++i)inventory.add(Item::Stone,64);
    check(!animals.tendAnimal(0,inventory,false,false) && inventory.count(Item::Bucket)==2 && animals.animals_[0].resourceTimer==0,"full_inventory_milk_refuses_without_loss");
    inventory.consumeSelected();check(animals.tendAnimal(0,inventory,false,false) && inventory.selectedItem()==Item::MilkBucket,"full_inventory_single_bucket_replaced");
    animals.animals_[2].resourceTimer=.01f;
    const auto dropCount=animals.drops_.size();animals.updateAnimal(animals.animals_[2],.02f,w,player,inventory,1);
    check(animals.drops_.size()==dropCount+1 && animals.drops_.back().stack.item==Item::Egg && animals.animals_[2].resourceTimer>=300,"adult_egg_laying");
    check(animals.save("animal-resources.vxm",991),"animal_resources_save");SurvivalWorld resources(991);
    check(resources.load("animal-resources.vxm",991) && resources.animals_[1].sheared && resources.animals_[1].woolTimer==180 && resources.animals_[0].resourceTimer==60 && resources.animals_[2].resourceTimer>=300 && !resources.animals_[2].persistent,"sheep_milk_egg_timers_and_natural_flags_reload");
    animals.animals_[1].woolTimer=.01f;w.setBlock(3,299,0,Block::Grass);
    animals.updateAnimal(animals.animals_[1],.02f,w,player,inventory,1);check(!animals.animals_[1].sheared,"grass_wool_regrowth");
    inventory.clear();inventory.add(Item::Shears,1,1);
    check(animals.tendAnimal(1,inventory,false,false) && inventory.selectedStack().empty() && animals.animals_[1].sheared,"shears_last_use_breaks_after_harvest");
    animals.animals_[0].resourceTimer=0;inventory.clear();inventory.add(Item::Bucket);
    check(animals.tendAnimal(0,inventory,false,true) && inventory.count(Item::Bucket)==1 && inventory.count(Item::MilkBucket)==1,"creative_milking_retains_bucket");
    animals.animals_[2].age=-10;animals.animals_[2].resourceTimer=0;const auto noEgg=animals.drops_.size();animals.updateAnimal(animals.animals_[2],.02f,w,player,inventory,1);check(animals.drops_.size()==noEgg,"chicks_no_eggs");
    animals.animals_.clear();for(int i=0;i<2;++i){SurvivalWorld::Animal a;a.type=SurvivalWorld::AnimalType::Chicken;a.position={float(i*2),300,0};a.home=a.position;a.health=4;animals.animals_.push_back(a);}
    check(!animals.feedAnimal({0,300.5f,3},{0,0,-1},Item::Wheat,&w),"chicken_wrong_food");
    check(animals.feedAnimal({0,300.5f,3},{0,0,-1},Item::Seeds,&w),"chicken_feed");
    check(!animals.feedAnimal({0,300.5f,3},{0,0,-1},Item::Seeds,&w),"love_no_repeat");
    check(animals.feedAnimal({2,300.5f,3},{0,0,-1},Item::BeetrootSeeds,&w) && animals.animals_.size()==3 && animals.animals_[2].age<0 && animals.animals_[0].persistent && animals.animals_[1].persistent && animals.animals_[2].persistent,"breeding_one_persistent_chick");
    check(animals.animals_[0].breedingCooldown==300 && animals.animals_[1].breedingCooldown==300,"breeding_cooldown");
    check(animals.save("farm-animals.vxm",991),"husbandry_save");SurvivalWorld reloaded(991);
    check(reloaded.load("farm-animals.vxm",991) && reloaded.animals_.size()==3 && reloaded.animals_[0].breedingCooldown==300 && reloaded.animals_[2].age<0,"husbandry_reload");
    check(reloaded.load("farm-animals.vxm",991) && reloaded.animals_.size()==3,"animal_reload_no_duplication");
    const auto husbandry=bytes("farm-animals.vxm");auto invalidAnimal=husbandry;const auto tag=invalidAnimal.rfind("HUS1");
    std::memcpy(invalidAnimal.data()+tag+9,&nan,4);write("bad-husbandry.vxm",invalidAnimal);
    check(!reloaded.load("bad-husbandry.vxm",991) && reloaded.animals_.size()==3,"invalid_husbandry_transactional");
    animals.animals_[0].resourceTimer=nan;
    check(!animals.save("farm-animals.vxm",991) && bytes("farm-animals.vxm")==husbandry,"invalid_husbandry_writer_preserves_previous");
    animals.animals_[0].resourceTimer=300;
    // Complete checkpoint contains new crops, composter, nutrition and livestock.
    check(g.saveAll(),"farming_checkpoint");check(SaveFile::verifyCheckpoint(selected.directory.string()),"checkpoint_manifest");
    g.returnToMainMenu();g.playSavedWorld(selected);
    check(g.world_ && g.player_->hunger()==12 && g.player_->saturation()==3 && g.survival_->animals_.size()==3,"farming_checkpoint_reload");
    check(g.saveAll(true),"farm_background_snapshot");
    g.player_->eat(-2);g.player_->eat(1,1);g.world_->setBlock(8,300,6,Block::Composter3);
    g.setGameMode(GameMode::Creative);g.setGameMode(GameMode::Spectator);g.setGameMode(GameMode::Survival);
    check(g.saveAll(true),"farm_background_latest_snapshot");g.pollSave(true);
    check(SaveFile::verifyCheckpoint(selected.directory.string()),"farm_background_integrity");
    g.returnToMainMenu();g.playSavedWorld(selected);g.world_->prepareSpawnTerrain({8,300,6});
    check(g.player_->hunger()==11 && g.player_->saturation()==4 && g.world_->getBlock(8,300,6)==Block::Composter3,"farm_background_latest_reload");
    g.player_->killByCommand();check(g.saveAll(),"farm_dead_checkpoint");g.returnToMainMenu();g.playSavedWorld(selected);
    check(g.player_->isDead() && g.player_->hunger()==11 && SaveFile::verifyCheckpoint(selected.directory.string()),"farm_dead_reload_integrity");
    {
        World edge(991,22);edge.generate(2,{.5f,300,.5f});edge.prepareSpawnTerrain({16.5f,300,4.5f});
        edge.setBlock(15,299,4,Block::WetFarmland);edge.setBlock(15,300,4,Block::MelonStem3);
        edge.setBlock(14,299,4,Block::Dirt);edge.setBlock(14,300,4,Block::Air);
        edge.setBlock(16,299,4,Block::Dirt);edge.setBlock(16,300,4,Block::Melon);
        edge.generate(2,{.5f,300,.5f});FarmingSystem boundary;
        check(!edge.hasLoadedChunkAt(16,4) && edge.getBlock(15,300,4)==Block::MelonStem3,"fruit_boundary_unloaded_fixture");
        for(int i=0;i<10800;++i)boundary.update(1.f/120,edge,{.5f,300,.5f});
        check(edge.getBlock(14,300,4)==Block::Air,"fruit_defers_unknown_neighbors");
        edge.prepareSpawnTerrain({16.5f,300,4.5f});
        for(int i=0;i<10800;++i)boundary.update(1.f/120,edge,{.5f,300,.5f});
        check(edge.getBlock(16,300,4)==Block::Melon && edge.getBlock(14,300,4)==Block::Air,"fruit_reload_no_duplicate");
        edge.setBlock(16,300,4,Block::Air);
        for(int i=0;i<10800;++i)boundary.update(1.f/120,edge,{.5f,300,.5f});
        check((edge.getBlock(16,300,4)==Block::Melon)+(edge.getBlock(14,300,4)==Block::Melon)==1,"fruit_boundary_regrows_once");
    }
    for(int id=174;id<int(Item::Count);++id){Item restored;check(itemFromSaveId(std::uint8_t(id),restored) && int(restored)==id && !itemDefinition(restored).displayName.empty(),"item_registry_"+std::to_string(id));}
    for(int id=167;id<int(Block::Count);++id){Block restored;check(blockFromSaveId(std::uint8_t(id),restored) && int(restored)==id && blockDefinition(restored).topTexture>=106,"block_registry_"+std::to_string(id));}
    std::cout<<"Farming assertions="<<assertions<<std::endl;g.shutdown();return 0;
}

int FarmingTest::showcase(Game& g,const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    auto& w=*g.world_;const auto scene=w.optimizationScenes()[0];
    w.generate(8,scene);const int x=int(std::floor(scene.x)),z=int(std::floor(scene.z)),y=int(std::floor(scene.y));
    for(int dx:{-8,8,24})for(int dz:{-8,8,24})w.prepareSpawnTerrain({float(x+dx),float(y),float(z+dz)});
    w.fillBlocks({x-7,y-1,z-7},{x+22,y-1,z+22},Block::Grass);
    w.fillBlocks({x-7,y,z-7},{x+22,y+5,z+22},Block::Air);
    for(int row=0;row<6;++row)for(int col=0;col<12;++col) {
        const int px=x+col,pz=z+row*2;
        w.setBlock(px,y-1,pz,Block::WetFarmland);
        const Block base=row==0?Block::Crop0:Block(int(Block::Carrot0)+(row-1)*4);
        w.setBlock(px,y,pz,Block(int(base)+std::min(3,col/3)));
    }
    for(int col=0;col<12;++col)w.setBlock(x+col,y-1,z-1,Block::Water);
    for(int i=0;i<4;++i){w.setBlock(x+14,y,z+i*2,Block(int(Block::BerryBush0)+i));w.setBlock(x+16,y,z+i*2,Block(int(Block::Composter0)+i));}
    w.setBlock(x+3,y,z+9,Block::Pumpkin);w.setBlock(x+7,y,z+11,Block::Melon);
    for(int i=0;i<5;++i){w.setBlock(x+19,y-1,z+i,Block::Water);w.setBlock(x+18,y-1,z+i,Block::Sand);for(int h=0;h<1+i%3;++h)w.setBlock(x+18,y+h,z+i,Block::SugarCane);}
    w.setBlock(x+15,y,z+9,Block::CraftingTable);w.setBlock(x+16,y,z+9,Block::Furnace);w.setBlock(x+17,y,z+9,Block::Chest);
    g.survival_->animals_.clear();
    for(int i=0;i<7;++i) {
        SurvivalWorld::Animal a;a.type=i<4?SurvivalWorld::AnimalType::Chicken:i==4?SurvivalWorld::AnimalType::Cow:i==5?SurvivalWorld::AnimalType::Sheep:SurvivalWorld::AnimalType::Pig;
        a.position={float(x+13+i%3*2),float(y),float(z+12+i/3*2)};a.home=a.position;a.heading={0,0};a.thinkTimer=100;a.persistent=true;a.resourceTimer=500;
        if(i==1)a.age=-500;if(i==5){a.sheared=true;a.woolTimer=180;}g.survival_->animals_.push_back(a);
    }
    g.inventory_->clear();for(int id=174;id<int(Item::Count);++id)g.inventory_->add(Item(id),std::min(8,Inventory::maxStack(Item(id))));
    g.settings_.vsync=false;g.settings_.frameLimit=120;g.showDebug_=false;g.timing_.worldTime=300;
    glfwSwapInterval(0);glfwShowWindow(g.window_);glfwFocusWindow(g.window_);
    auto view=[&](const char* label,glm::vec3 p,float yaw,float pitch,bool inventory) {
        g.ui_.closeGameplayInterface(*g.inventory_);g.player_->teleport(p);g.player_->yaw_=yaw;g.player_->pitch_=pitch;
        if(inventory){
            g.ui_.openInventory();const_cast<RecipeBookView&>(g.ui_.recipeBook()).search="stew";
            int width=0,height=0;glfwGetFramebufferSize(g.window_,&width,&height);
            g.ui_.updateInventoryInteraction(g.input_,width,height,*g.inventory_,*g.survival_,w,*g.player_,false);
        }
        const double start=glfwGetTime();
        while(glfwGetTime()-start<4) {glfwPollEvents();w.setStreamingView(g.player_->lookDirection(),5);w.updateStreaming(g.player_->cameraPosition(),8);g.survival_->update(.008f,w,*g.player_,*g.inventory_,1);g.sounds_->update(.008f);g.renderFrame(.008f);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        const auto prior=std::filesystem::exists("screenshots")?std::distance(std::filesystem::directory_iterator("screenshots"),std::filesystem::directory_iterator{}):0;
        g.screenshotRequested_=true;g.renderFrame(0);
        std::filesystem::path newest;for(auto& file:std::filesystem::directory_iterator("screenshots"))if(newest.empty() || file.last_write_time()>std::filesystem::last_write_time(newest))newest=file.path();
        check(!newest.empty(),std::string("screenshot_")+label);std::filesystem::copy_file(newest,output/(std::string(label)+".bmp"),std::filesystem::copy_options::overwrite_existing);
    };
    view("farm-overview",{float(x+23),float(y+12),float(z+23)},-135,-33,false);
    view("farm-ground",{float(x+12),float(y+1),float(z+15)},-125,-15,false);
    view("chickens-livestock",{float(x+18),float(y),float(z+18)},-105,-14,false);
    view("farming-inventory",{float(x+12),float(y+1),float(z+15)},-125,-15,true);
    check(g.saveAll(),"showcase_checkpoint");g.shutdown();return 0;
}

int FarmingTest::stress(Game& g,const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    std::ofstream csv(output/"crop-cpu.csv");csv<<"variant,crops,state,repeat,steps,mean_ms,p95_ms,p99_ms,worst_ms\n";
    auto& w=*g.world_;w.setSimulationDistance(7);w.setSimulationDistance(8);
    for(int x=0;x<=112;x+=16)for(int z=0;z<=112;z+=16)w.prepareSpawnTerrain({float(x),300,float(z)});
    w.fillBlocks({0,299,0},{100,299,100},Block::Dirt);
    w.fillBlocks({0,300,0},{100,301,100},Block::Air);
    auto stats=[&](const char* variant,int count,const char* state,int repeat,std::vector<double>& times) {
        double total=0;for(auto t:times)total+=t;std::sort(times.begin(),times.end());
        csv<<variant<<','<<count<<','<<state<<','<<repeat<<','<<times.size()<<','<<total/times.size()<<','<<times[times.size()*95/100]<<','<<times[times.size()*99/100]<<','<<times.back()<<'\n';csv.flush();
    };
    for(int count:{100,1000,10000})for(bool mature:{false,true})for(int repeat=0;repeat<3;++repeat)for(bool baseline:{true,false}) {
        FarmingSystem current;BaselineFarmingSystem original;
        w.fillBlocks({0,300,0},{100,300,100},Block::Air);
        // Each sample has exactly count farmland cells. Leaving a prepared
        // 10,000-cell empty tilled field in the 100-crop case would measure
        // additional soil simulation absent from the old wheat-only updater.
        w.fillBlocks({0,299,0},{100,299,100},Block::Dirt);
        for(int i=0;i<count;++i) {
            const glm::ivec3 p(i%100,300,i/100);
            w.setBlock(p.x,p.y-1,p.z,Block::Farmland);
            if(baseline)original.plant(w,p);else current.plant(w,p);
            if(mature)w.setBlock(p.x,p.y,p.z,Block::Crop3);
        }
        // Settle mesh work before timing CPU crop work. No quality reductions.
        for(int i=0;i<600;++i) {if(baseline)original.update(1.f/120,w,{50,300,50});else current.update(1.f/120,w,{50,300,50});}
        std::vector<double> times;times.reserve(1440);
        for(int i=0;i<1440;++i) {
            auto start=std::chrono::steady_clock::now();
            if(baseline)original.update(1.f/120,w,{50,300,50});else current.update(1.f/120,w,{50,300,50});
            times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        stats(baseline?"v2.14.3-source":"v2.15.0",count,mature?"mature":"growing",repeat,times);
    }
    std::ofstream animalCsv(output/"animal-cpu.csv");animalCsv<<"animals,repeat,steps,mean_ms,p95_ms,p99_ms,worst_ms\n";
    auto& s=*g.survival_;auto& p=*g.player_;p.teleport({50,300,50});g.inventory_->clear();
    for(int count:{10,50,100,200})for(int repeat=0;repeat<3;++repeat) {
        s.animals_.clear();s.drops_.clear();s.spawnAccumulator_=0;s.passiveSpawnCooldown_=1000;
        for(int i=0;i<count;++i) {SurvivalWorld::Animal a;a.type=i%4==3?SurvivalWorld::AnimalType::Chicken:SurvivalWorld::AnimalType(i%4);a.position={float(8+i%20*4),300,float(8+i/20*4)};a.home=a.position;a.persistent=true;a.health=8;a.resourceTimer=500;s.animals_.push_back(a);}
        std::vector<double> times;
        for(int i=0;i<1440;++i) {auto start=std::chrono::steady_clock::now();s.update(1.f/120,w,p,*g.inventory_,1);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
        double total=0;for(auto t:times)total+=t;std::sort(times.begin(),times.end());animalCsv<<count<<','<<repeat<<','<<times.size()<<','<<total/times.size()<<','<<times[times.size()*95/100]<<','<<times[times.size()*99/100]<<','<<times.back()<<'\n';animalCsv.flush();
    }
    // Frame rate changes only the number of render frames; fixed 120Hz steps
    // execute the identical elapsed crop work, including bounded catch-up.
    w.fillBlocks({0,300,0},{100,300,100},Block::Air);
    std::ofstream timing(output/"timing.csv");timing<<"fps,steps,crop_stage\n";
    for(int fps:{10,15,20,30,60,120}) {
        FarmingSystem f;w.setBlock(10,300,10,Block::Crop0);SimulationClock clock;int steps=0;
        for(int frame=0;frame<fps*150;++frame)for(unsigned step=0,n=clock.advance(1./fps);step<n;++step){f.update(float(SimulationClock::Step),w,{10,300,10});++steps;}
        const auto b=w.getBlock(10,300,10);check(b==Block::Crop3,"fixed_step_growth_"+std::to_string(fps));timing<<fps<<','<<steps<<','<<cropStage(b)<<'\n';
    }
    std::cout<<"Crop/animal CPU evidence: "<<output<<std::endl;g.shutdown();return 0;
}
