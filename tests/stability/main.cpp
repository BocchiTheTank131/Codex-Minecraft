#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "World.h"
#include "Player.h"
#include "Farming.h"
#include "Survival.h"
#include "Settings.h"
#include "WorldLibrary.h"
#include "Transportation.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <cstring>
#include <chrono>
#include "InputManager.h"
#include "UIManager.h"
#include "CommandSystem.h"
#include "ChatUI.h"
#include "Game.h"
#include "SaveFile.h"
#include "Weather.h"
#include "Sound.h"
#include "SimulationClock.h"
#include "Renderer.h"
#include <thread>
#include <algorithm>
#include <map>
#ifdef _WIN32
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <psapi.h>
#endif
#include <filesystem>
#include <stdexcept>
class StabilityTest {
public:
static int run(int argc,char** argv);
static int traversal(int rd,const std::filesystem::path& output);
static bool farmingControls(Game&);
};
int runFarmingTests(int argc,char** argv);
bool testFarmingControls(Game& g){return StabilityTest::farmingControls(g);}

bool StabilityTest::farmingControls(Game& g) {
 auto& w=*g.world_;auto& inventory=*g.inventory_;const glm::ivec3 soil(2,299,4);
 w.setBlock(soil.x,soil.y,soil.z,Block::Dirt);w.setBlock(soil.x,soil.y+1,soil.z,Block::Air);
 auto frame=[&]{g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.mouseButtons_[GLFW_MOUSE_BUTTON_RIGHT]=true;g.interaction_.mobTarget={};};
 auto shield=[&](Item held){inventory.clear();inventory.add(Item::Shield);inventory.equipFromSlot(0);inventory.add(held,1);};
 frame();shield(Item::WoodHoe);g.interaction_.hasBlockTarget=true;g.interaction_.blockTarget={soil,soil+glm::ivec3(0,1,0),{0,1,0},3};
 g.updateEquipmentInput(true);g.handleUseAction();bool ok=isFarmland(w.getBlock(soil.x,soil.y,soil.z)) && inventory.selectedStack().durability==Inventory::maxDurability(Item::WoodHoe)-1;
 frame();shield(Item::Carrot);g.player_->eat(20);g.updateEquipmentInput(true);g.handleUseAction();ok &= inventory.selectedStack().empty() && w.getBlock(soil.x,soil.y+1,soil.z)==Block::Carrot0 && g.player_->hunger()==20;
 frame();shield(Item::MilkBucket);g.player_->eat(-10);g.updateEquipmentInput(true);g.handleUseAction();ok &= inventory.selectedItem()==Item::Bucket;
 frame();shield(Item::WoodHoe);g.interaction_.hasBlockTarget=false;g.updateEquipmentInput(true);ok &= g.player_->isBlocking();
 g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.player_->setBlocking(false);
 return ok;
}
int main(int argc,char** argv){
    // This harness cannot operate on the current profile. Every run gets a new
    // isolated fixture; evidence is kept for inspection, never auto-deleted.
    auto root=std::filesystem::temp_directory_path()/ ("VoxelFrontier-stability-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);std::filesystem::current_path(root);
    std::cout<<"Isolated fixture: "<<root<<std::endl;
    try{return StabilityTest::run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
int StabilityTest::run(int argc,char** argv){
 if(argc>1 && std::string(argv[1]).find("--farming")==0)return runFarmingTests(argc,argv);
 if(argc==2 && std::string(argv[1]).find("--session-probe=")==0){
    const auto target=std::filesystem::weakly_canonical(std::string(argv[1]).substr(16));
    if(target.parent_path()!=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()) || target.filename().string().find("VoxelFrontier-stability-")!=0)return 2;
    SaveFile::Session session(target.string());return session.acquired()?1:0;
 }
 if(argc>1 && std::string(argv[1])=="--traversal")return traversal(argc>2?std::stoi(argv[2]):8,argc>3?std::filesystem::path(argv[3]):std::filesystem::current_path());
 auto log=[](const char* name,bool passed){std::cout<<name<<" passed="<<passed<<std::endl;if(!passed)throw std::runtime_error(name);};
 if(argc>1 && std::string(argv[1])=="--checkpoint") {
  Game g;char app[]="checkpoint-regression";char* args[]={app};if(!g.initialize(1,args))return 5;glfwHideWindow(g.window_);
  const auto selected=WorldLibrary::create("Metadata regression",42,GameMode::Survival,true);
  g.playSavedWorld(selected);log("initial_world_loaded",bool(g.world_));log("initial_checkpoint",g.saveAll());
  g.setGameMode(GameMode::Creative);
  log("gamemode_preserves_manifest",SaveFile::verifyCheckpoint(selected.directory.string()));
  log("gamemode_checkpoint",g.saveAll());
  auto bytes=[](const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
  auto snapshot=[&](const std::filesystem::path& dir){std::map<std::string,std::string> state;for(const auto& p:std::filesystem::directory_iterator(dir))if(p.is_regular_file() && p.path().filename()!=".session.lock")state[p.path().filename().string()]=bytes(p.path());return state;};
  auto command=[&](const std::string& text){CommandContext c{*g.world_,*g.player_,*g.inventory_,*g.survival_,g.timing_.worldTime,42,[&]{return g.gameMode();},[&](GameMode m){g.setGameMode(m);},g.activeWorld_.cheats,g.weather_.get(),g.sounds_.get()};return g.commands_.execute(text,c).tone!=ChatTone::Error;};
  for(const auto& mode:{std::string("spectator"),std::string("survival"),std::string("creative")}){
    const auto disk=bytes(selected.directory/"world_seed.txt");
    log("mode_time_weather_commands",command("/gamemode "+mode)&&command("/time set midnight")&&command("/weather thunder 180"));
    log("commands_only_change_memory",bytes(selected.directory/"world_seed.txt")==disk && SaveFile::verifyCheckpoint(selected.directory.string()));
    log("mode_autosave_queued",g.saveAll(true));g.pollSave(true);
    log("mode_autosave_valid",SaveFile::verifyCheckpoint(selected.directory.string()));
    g.returnToMainMenu();g.playSavedWorld(selected);
    log("mode_time_weather_reload",g.world_ && g.gameMode()==(mode=="spectator"?GameMode::Spectator:mode=="creative"?GameMode::Creative:GameMode::Survival) && g.timing_.worldTime==WorldTime::MidnightSeconds && g.weather_->type()==WeatherType::Thunder);
  }
  log("background_first_snapshot",command("/time set noon")&&g.saveAll(true));
  log("background_pending_change",command("/gamemode survival")&&command("/time set night")&&command("/weather rain 240"));
  log("background_save_coalesced",g.saveAll(true)&&g.saveQueued_);g.pollSave(true);
  const auto persisted=WorldLibrary::inspectReserved(selected.directory);
  log("background_latest_state_committed",persisted.mode==GameMode::Survival && persisted.time==WorldTime::NightSeconds && SaveFile::verifyCheckpoint(selected.directory.string()));
  log("owned_metadata_cannot_bypass_session",!WorldLibrary::setCheats(selected,false));
  log("active_metadata_serializer_guard",!g.saveWorldMetadata());
  const auto beforeGuard=snapshot(selected.directory);
  for(const char* file:{"world_seed.txt","world.info","voxel_world.vxw","voxel_inventory.vxi","voxel_player.vps","voxel_mobs.vxm","voxel_travel.vtn","checkpoint.manifest"})
    log("standalone_managed_write_refused",!SaveFile::write((selected.directory/file).string(),std::ios::binary,[](std::ofstream& f){f<<"bad";return true;}));
  log("standalone_guard_preserves_all_files",snapshot(selected.directory)==beforeGuard);
  // A stale selection may only change its intended permission, never older metadata.
  g.returnToMainMenu();auto stale=selected;stale.name="Stale name";stale.lastPlayed=0;
  log("cheats_transaction",WorldLibrary::setCheats(stale,false));
  auto listed=WorldLibrary::list();auto found=std::find_if(listed.begin(),listed.end(),[&](const auto& w){return w.directory==selected.directory;});
  log("cheats_preserve_current_metadata",found!=listed.end()&&!found->cheats&&found->name==selected.name&&found->lastPlayed>0&&found->mode==GameMode::Survival);
  g.playSavedWorld(*found);log("cheats_executor_denies_command",!command("/time set noon"));g.returnToMainMenu();
  log("cheats_reenabled",WorldLibrary::setCheats(selected,true));g.playSavedWorld(selected);
  log("shutdown_async_snapshot",command("/time set 456")&&g.saveAll(true));g.shutdown();
  Game restarted;if(!restarted.initialize(1,args))return 5;glfwHideWindow(restarted.window_);restarted.playSavedWorld(selected);
  log("shutdown_reload_latest_checkpoint",restarted.world_&&restarted.timing_.worldTime==456&&restarted.gameMode()==GameMode::Survival&&restarted.weather_->type()==WeatherType::Rain);
  log("all_background_music_playable",!restarted.sounds_->currentSongName().empty()&&restarted.sounds_->verifyMusic());
  // Deterministically introduce external/old-client corruption before the worker
  // verifies its immutable snapshot. The worker must classify it as permanent.
  auto status=std::make_shared<SaveFile::CommitStatus>();
  {SaveFile::Checkpoint batch((selected.directory/"save.checkpoint").string());log("worker_fixture_ready",batch.ready());
   log("worker_fixture_staged",SaveFile::write((selected.directory/"world_seed.txt").string(),std::ios::out,[](std::ofstream& f){f<<"42\nsurvival\n500\nTIME_CYCLE1 1200\n";return true;}));
   {std::ofstream legacy(selected.directory/"world_seed.txt");legacy<<"42\ncreative\n600\nTIME_CYCLE1 1200\n";}
   restarted.pendingSaveStatus_=status;restarted.pendingSave_=batch.commitAsync(status);
  }
  restarted.pollSave(true);const auto damaged=snapshot(selected.directory);
  log("worker_integrity_failure_classified",status->permanentFailure&&status->reason.find("world_seed.txt")!=std::string::npos&&!restarted.saveBlockedReason_.empty());
  for(int n=0;n<20;++n)log("permanent_failure_no_retry",!restarted.saveAll(n%2!=0)&&restarted.saveRetryAt_==0);
  restarted.timing_.lastSave=glfwGetTime()-20;restarted.updateSimulation(1.f/60);
  log("autosave_does_not_rearm_integrity_retry",restarted.saveRetryAt_==0&&!restarted.pendingSave_.valid()&&snapshot(selected.directory)==damaged);
  restarted.resetWorld(99,GameMode::Survival);log("reset_does_not_overwrite_damaged_world",restarted.seed_==42&&snapshot(selected.directory)==damaged);
  restarted.shutdown();
  const auto sourceBefore=snapshot(selected.directory);const auto recovered=WorldLibrary::recoverPrevious(selected);
  log("recovery_creates_verified_separate_copy",recovered.directory!=selected.directory&&SaveFile::verifyCheckpoint(recovered.directory.string())&&snapshot(selected.directory)==sourceBefore);
  const auto noBackup=WorldLibrary::create("No complete backup",43,GameMode::Survival);
  const auto noBackupBefore=snapshot(noBackup.directory);bool refused=false;try{WorldLibrary::recoverPrevious(noBackup);}catch(const std::exception&){refused=true;}
  log("missing_verified_backup_refused",refused&&snapshot(noBackup.directory)==noBackupBefore);
  auto invalidBackup=selected;invalidBackup.directory=selected.directory.parent_path()/"invalid-backup-copy";std::filesystem::create_directory(invalidBackup.directory);
  for(const auto& entry:sourceBefore){std::ofstream f(invalidBackup.directory/entry.first,std::ios::binary);f.write(entry.second.data(),std::streamsize(entry.second.size()));}
  {std::ofstream f(invalidBackup.directory/"world_seed.txt.checkpoint-old");f<<"unverified";}
  const auto invalidBefore=snapshot(invalidBackup.directory);refused=false;try{WorldLibrary::recoverPrevious(invalidBackup);}catch(const std::exception&){refused=true;}
  log("invalid_backup_refused_without_repair",refused&&snapshot(invalidBackup.directory)==invalidBefore);
  // Make a pre-manifest compatibility fixture from a verified copy; never
  // strip or modify the original world's manifest.
  auto legacy=selected;legacy.directory=selected.directory.parent_path()/"legacy-copy";std::filesystem::create_directory(legacy.directory);
  for(const char* name:{"world.info","world_seed.txt","voxel_world.vxw","voxel_inventory.vxi","voxel_player.vps","voxel_mobs.vxm","voxel_travel.vtn"})std::filesystem::copy_file(recovered.directory/name,legacy.directory/name);
  {std::ofstream info(legacy.directory/"world.info");info<<"\"Legacy copy\"\n0\n";}
  Game legacyGame;if(!legacyGame.initialize(1,args))return 5;glfwHideWindow(legacyGame.window_);legacyGame.playSavedWorld(legacy);
  log("legacy_copy_loads",bool(legacyGame.world_));legacyGame.setGameMode(GameMode::Creative);log("legacy_copy_migrates_transactionally",legacyGame.saveAll()&&SaveFile::verifyCheckpoint(legacy.directory.string()));legacyGame.returnToMainMenu();legacyGame.playSavedWorld(legacy);log("legacy_copy_reloads",legacyGame.world_&&legacyGame.gameMode()==GameMode::Creative);legacyGame.returnToMainMenu();legacyGame.playSavedWorld(recovered);log("recovered_copy_loads",bool(legacyGame.world_));legacyGame.returnToMainMenu();legacyGame.shutdown();
  log("affected_originals_still_identical",snapshot(selected.directory)==sourceBefore);
  const auto originalCwd=std::filesystem::current_path();
  const auto importFixture=originalCwd/"root-import-copy";std::filesystem::create_directory(importFixture);
  for(const char* name:{"voxel_world.vxw","voxel_inventory.vxi","voxel_player.vps","voxel_mobs.vxm","voxel_travel.vtn"})std::filesystem::copy_file(recovered.directory/name,importFixture/name);
  const auto originalRootFiles=snapshot(importFixture);std::filesystem::current_path(importFixture);
  WorldLibrary::importLegacy();const auto imported=WorldLibrary::list();
  log("root_legacy_import_transaction",imported.size()==1&&imported[0].seed==42&&SaveFile::verifyCheckpoint(imported[0].directory.string()));
  for(const auto& original:originalRootFiles)log("root_legacy_original_preserved",bytes(original.first)==original.second);
  std::filesystem::current_path(originalCwd);
  return 0;
 }
 if(argc>1 && std::string(argv[1])=="--game") {
  Game g;char arg[]="audit";char* args[]={arg};if(!g.initialize(1,args))return 5;glfwHideWindow(g.window_);
  const auto selected=WorldLibrary::create("Game audit",42,GameMode::Survival,false);g.playSavedWorld(selected);if(!g.world_)return 6;
  g.player_->damage(7);g.player_->eat(-10);g.saveAll();g.returnToMainMenu();g.playSavedWorld(selected);log("actual_game_statistics_preserved",g.player_->health()==13&&g.player_->hunger()==10);
  const auto cell=glm::ivec3(glm::floor(g.player_->position()));g.world_->setBlock(cell.x,300,cell.z,Block::Air);g.saveAll();g.world_->setBlock(cell.x,300,cell.z,Block::Stone);g.inventory_->add(Item::Diamond);g.player_->killByCommand();bool saved=g.saveAll();{World check(42,22);glm::vec3 pos;check.loadWorld((selected.directory/"voxel_world.vxw").string(),pos);check.generate(2,pos);Inventory inv;inv.load((selected.directory/"voxel_inventory.vxi").string(),42);log("dead_save_coherent",saved&&check.getBlock(cell.x,300,cell.z)==Block::Stone&&inv.count(Item::Diamond)==1);}

  g.player_->setCreativeMode(false);
  for(int fps:{10,15,20,30,60,120}){
    const float start=g.timing_.worldTime;g.simulationClock_.reset();
    for(int frame=0;frame<fps*3;++frame)g.updateSimulation(1.f/fps);
    log(("game_fixed_clock_"+std::to_string(fps)).c_str(),std::abs(g.timing_.worldTime-start-3)<.02f);
  }
  {const float start=g.timing_.worldTime;g.updateSimulation(1.f);for(int n=0;n<120;++n)g.updateSimulation(1.f/120);log("one_second_spike_debt_recovered",g.simulationClock_.debt<.009 && std::abs(g.timing_.worldTime-start-2)<.02f);}
  g.timing_.previousFrame=glfwGetTime()-.1;log("raw_frame_metric",g.beginFrame()>.09f && g.timing_.frameMilliseconds>90);
  {CommandContext c{*g.world_,*g.player_,*g.inventory_,*g.survival_,g.timing_.worldTime,42,[&]{return g.gameMode();},[&](GameMode m){g.setGameMode(m);},true};
    g.world_->prepareSpawnTerrain({.5f,200,.5f});log("teleport_origin_loaded_precondition",g.world_->hasLoadedChunkAt(0,0));
    const auto start=std::chrono::steady_clock::now();auto r=g.commands_.execute("/tp 1600016.5 200 0.5",c);
    log("command_teleport_accepted",r.tone!=ChatTone::Error);const float y=g.player_->position().y;
    g.updateSimulation(1.f/60);log("command_teleport_waits_for_collision",g.player_->position().y==y && !g.world_->hasLoadedChunkAt(0,0) && !g.world_->simulationActiveAt(0,0));
    std::cout<<"command_teleport_frame_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<std::endl;
  }
  g.survival_=std::make_unique<SurvivalWorld>(42);g.survival_->setSoundSystem(g.sounds_.get());g.survival_->setWeather(g.weather_.get());
  for(int n=0;n<5001;++n){g.survival_->summonMob("cow",{0,80,0});g.survival_->spawnDrop({0,80,0},Item::Stone);}
  log("game_large_entity_checkpoint",g.saveAll());g.returnToMainMenu();g.playSavedWorld(selected);
  log("game_large_entity_reload",g.world_ && g.survival_->diagnostics(g.player_->position()).passive==5001 && g.survival_->renderItemSprites().size()==5001);
  {auto& a=g.survival_->animals_.front();a.persistent=false;a.age=-30;g.saveAll();g.returnToMainMenu();g.playSavedWorld(selected);log("animal_persistence_and_baby_age",!g.survival_->animals_.front().persistent && g.survival_->animals_.front().age==-30);}
  log("game_async_snapshot_staged",g.saveAll(true));g.inventory_->add(Item::GoldIngot);g.pollSave(true);
  {Inventory previous;log("async_capture_immutable",previous.load((selected.directory/"voxel_inventory.vxi").string(),42)&&previous.count(Item::GoldIngot)==0);}
  g.returnToMainMenu();
  for(const char* name:{"voxel_inventory.vxi","voxel_player.vps","voxel_mobs.vxm"}){
    const auto file=selected.directory/name;const auto backup=file.string()+".fixture-good";std::filesystem::copy_file(file,backup);
    // Simulate a legacy world with no manifest, exercising each real decoder.
    std::filesystem::rename(selected.directory/"checkpoint.manifest",selected.directory/"manifest.fixture-good");
    {std::ofstream bad(file,std::ios::binary);bad<<"broken";}
    g.playSavedWorld(selected);log((std::string("legacy_corrupt_refused_")+name).c_str(),!g.world_&&!g.menuMessage_.empty());g.saveAll();
    std::ifstream bytes(file,std::ios::binary);std::string content((std::istreambuf_iterator<char>(bytes)),{});bytes.close();log("corrupt_bytes_preserved",content=="broken");
    std::filesystem::copy_file(backup,file,std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(selected.directory/"manifest.fixture-good",selected.directory/"checkpoint.manifest");
  }
  {std::ofstream bad(selected.directory/"voxel_inventory.vxi",std::ios::binary);bad<<"broken";}
  g.playSavedWorld(selected);log("manifest_corruption_refused",!g.world_);g.refreshWorlds();
  log("game_world_selection_retains_unavailable",std::any_of(g.savedWorlds_.begin(),g.savedWorlds_.end(),[](const auto& w){return !w.loadError.empty();}));
  for(int n=0;n<int(g.savedWorlds_.size());++n)if(!g.savedWorlds_[n].loadError.empty())g.selectedWorld_=n;
  const auto worldCount=g.savedWorlds_.size();g.input_.mouseButtons_.fill(false);g.input_.previousMouseButtons_.fill(false);g.input_.keys_.fill(false);g.input_.previousKeys_.fill(false);
  g.input_.keys_[GLFW_KEY_R]=true;g.updatePauseInterface();
  log("keyboard_recovery_requires_confirmation",g.recoverWorldPending_>=0 && g.savedWorlds_.size()==worldCount);
  g.input_.previousKeys_[GLFW_KEY_R]=true;g.input_.keys_[GLFW_KEY_R]=false;g.updatePauseInterface();
  g.input_.previousKeys_[GLFW_KEY_R]=false;g.input_.keys_[GLFW_KEY_R]=true;g.updatePauseInterface();g.input_.keys_[GLFW_KEY_R]=false;
  log("keyboard_recovery_creates_separate_world",g.savedWorlds_.size()==worldCount+1);
  SavedWorld recovered;for(const auto& w:g.savedWorlds_)if(w.name.rfind("Recovered ",0)==0)recovered=w;
  log("recovery_copy_verified",!recovered.directory.empty() && SaveFile::verifyCheckpoint(recovered.directory.string()));
  g.playSavedWorld(recovered);log("recovery_copy_game_loads",bool(g.world_));g.returnToMainMenu();
  g.shutdown();return 0;
 }
 {SimulationTime clock;float value=WorldTime::DefaultSeconds;for(int n=0;n<144000;++n)clock.advance(value,SimulationClock::Step);log("twenty_minute_precise_clock",std::abs(double(value)-1300)<1e-5);value=700;clock.advance(value,SimulationClock::Step);log("precise_clock_time_command",std::abs(value-700.f-float(SimulationClock::Step))<.001f);}
 {Player p({0,0,0});p.damage(7);p.eat(-10);p.save("player.vps",42);Player q({0,0,0});q.load("player.vps",42);std::cout<<"before_mode health="<<q.health()<<" hunger="<<q.hunger()<<std::endl;log("player_statistics_roundtrip",q.health()==13&&q.hunger()==10);}
 {GameSettings s;std::ofstream f("nan.cfg");f<<"fov=nan\nmouse_sensitivity=nan\nmaster_volume=nan\n";f.close();s.load("nan.cfg");log("settings_reject_nan",std::isfinite(s.fov)&&std::isfinite(s.mouseSensitivity)&&std::isfinite(s.masterVolume));}
 {SurvivalWorld s(42);for(int i=0;i<5001;++i)s.summonMob("cow",{0,80,0});bool saved=s.save("many-mobs.vxm",42);SurvivalWorld q(42);log("large_mobs_roundtrip",saved&&q.load("many-mobs.vxm",42));}
 {SurvivalWorld s(42);for(int i=0;i<5001;++i)s.spawnDrop({0,80,0},Item::Stone);bool saved=s.save("many-drops.vxm",42);SurvivalWorld q(42);log("large_drops_roundtrip",saved&&q.load("many-drops.vxm",42));}
 {SurvivalWorld s(42);s.spawnDrop({0,80,0},Item::Stone,65);bool saved=s.save("oversized-drop.vxm",42);SurvivalWorld q(42);log("oversized_drops_split",saved&&q.load("oversized-drop.vxm",42));}
 {SurvivalWorld s(42);s.spawnDrop({0,20000,0},Item::Stone,1,0,{0,-1800,0});log("falling_drop_valid",s.renderItemSprites().size()==1);s.spawnDrop({0,80,0},Item::Stone,1,0,{0,20000,0});log("invalid_velocity_admission",s.renderItemSprites().size()==1);SurvivalWorld q(42);log("falling_drop_roundtrip",s.save("falling.vxm",42)&&q.load("falling.vxm",42));}
 {SurvivalWorld s(42);log("nonfinite_mob_admission",!s.summonMob("cow",{std::numeric_limits<float>::quiet_NaN(),80,0}));
 s.summonMob("cow",{0,80,0});s.save("nan-mob.vxm",42);std::fstream f("nan-mob.vxm",std::ios::binary|std::ios::in|std::ios::out);f.seekp(17);float nan=std::numeric_limits<float>::quiet_NaN();f.write((const char*)&nan,4);f.close();SurvivalWorld q(42);log("nonfinite_mob_load_rejected",!q.load("nan-mob.vxm",42));}
 {SaveFile::Session first("."),second(".");log("world_session_exclusive",first.acquired()&&!second.acquired());}

#ifdef _WIN32
 {SaveFile::Session session(".");wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
  std::wstring args=L"\""+std::wstring(exe)+L"\" \"--session-probe="+std::filesystem::current_path().wstring()+L"\"";
  STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
  log("start_simultaneous_session",CreateProcessW(exe,args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0);
  DWORD code=1;auto wait=WaitForSingleObject(pi.hProcess,10000);GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);log("cross_process_session_exclusive",wait==WAIT_OBJECT_0 && code==0);
 }
#endif
 {Inventory inv;inv.add(Item::Stone,5);inv.save("bad-stack.vxi",42);std::fstream f("bad-stack.vxi",std::ios::binary|std::ios::in|std::ios::out);f.seekp(17);std::int16_t count=65;f.write((const char*)&count,2);f.close();log("invalid_inventory_no_partial_mutation",!inv.load("bad-stack.vxi",42)&&inv.count(Item::Stone)==5);}
 {Transportation nav({0,80,0});auto id=nav.createMap({0,80,0});nav.save("bad-nav.vtn",42);std::fstream f("bad-nav.vtn",std::ios::binary|std::ios::in|std::ios::out);f.seekp(12);float value=std::numeric_limits<float>::infinity();f.write((const char*)&value,4);f.close();log("invalid_navigation_no_partial_mutation",!nav.load("bad-nav.vtn",42)&&nav.map(id) && nav.originalSpawn().y==80);}
 {Player p({0,80,0});p.damage(3);p.eat(-4);p.save("partial.vps",42);std::filesystem::resize_file("partial.vps",17);log("player_failed_load_transactional",!p.load("partial.vps",42)&&p.health()==17&&p.hunger()==16);}

 {SurvivalWorld s(42);for(int n=0;n<5001;++n)s.summonMob("cow",{100000.f,80,100000.f});std::vector<RenderCuboid> output;
 double all=0,filtered=0;std::size_t before=0;for(int n=0;n<20;++n){auto start=std::chrono::steady_clock::now();s.renderCuboids(output);all+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();before=output.size();start=std::chrono::steady_clock::now();s.renderCuboids(output,{0,80,0},128);filtered+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
 log("distant_passive_preparation_filtered",before==5001*6 && output.empty());std::cout<<"passive_preparation_all_ms="<<all/20<<" filtered_ms="<<filtered/20<<std::endl;}
 {WorldLibrary::create("Healthy",42,GameMode::Survival,false);auto bad=WorldLibrary::create("Broken",43,GameMode::Survival,false);std::ofstream f(bad.directory/"save.checkpoint");f<<"broken";f.close();bool failed=false;try{auto all=WorldLibrary::list();failed=all.size()!=2 || std::count_if(all.begin(),all.end(),[](const auto& w){return !w.loadError.empty();})!=1;}catch(...){failed=true;}log("broken_world_listing_isolated",!failed);}
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);auto* window=glfwCreateWindow(64,64,"Audit",nullptr,nullptr);if(!window)return 3;glfwMakeContextCurrent(window);if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))return 4;
 {World w(42,22);glm::vec3 restored;const glm::vec3 highAltitude(.5f,20000,.5f);
 log("high_altitude_position_roundtrip",w.saveWorld("high-altitude.vxw",highAltitude) && w.loadWorld("high-altitude.vxw",restored) && restored==highAltitude);
 w.generate(2,{.5f,80,.5f});
 {FarmingSystem f;double total=0,worst=0;for(int i=0;i<20;++i){auto start=std::chrono::steady_clock::now();f.update(5.1f,w,{4,80,4});double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();total+=ms;worst=std::max(worst,ms);}std::cout<<"farming_scan_mean_ms="<<total/20<<" worst_ms="<<worst<<std::endl;}
 // Coordinate regression is independent of sunlight and growth duration. The
 // farming suite separately tests real fixed-step natural growth and darkness.
 w.setBlock(2,-11,2,Block::Farmland);w.setBlock(2,-10,2,Block::Air);w.setBlock(4,79,4,Block::Farmland);w.setBlock(4,80,4,Block::Air);FarmingSystem f;f.plant(w,{2,-10,2});f.plant(w,{4,80,4});log("negative_y_crop_fertilization",f.fertilize(w,{2,-10,2})&&f.fertilize(w,{4,80,4})&&cropStage(w.getBlock(2,-10,2))>0&&cropStage(w.getBlock(4,80,4))>0);
 {const glm::ivec3 distantCrop(2097154,-10,-18);w.prepareSpawnTerrain(glm::vec3(distantCrop));w.updateStreaming(glm::vec3(distantCrop),0);w.setBlock(distantCrop.x,distantCrop.y-1,distantCrop.z,Block::Farmland);w.setBlock(distantCrop.x,distantCrop.y,distantCrop.z,Block::Air);f.plant(w,distantCrop);log("far_crop_no_coordinate_alias",f.fertilize(w,distantCrop)&&cropStage(w.getBlock(distantCrop.x,distantCrop.y,distantCrop.z))>0);w.prepareSpawnTerrain({4,80,4});w.updateStreaming({4,80,4},0);}
 Player p({10000.5f,80,10000.5f});for(int i=0;i<20;++i)p.update(.05f,{},w);log("unloaded_collision_waits",p.position().y==80);
 w.fillBlocks({6,80,8},{10,80,8},Block::Air);w.setBlock(8,80,8,Block::StoneSlab);RayHit hit;log("raycast_respects_slab",!w.raycast({6.5f,80.8f,8.5f},{1,0,0},4,hit));
 w.updateStreaming({1048576.5f,80,.5f},0);log("far_teleport_unloads_origin",!w.hasLoadedChunkAt(0,0));
 w.generate(2,{1600016.5f,80,.5f});log("far_simulation_radius_preserved",!w.simulationActiveAt(0,0));
 Transportation t({0,80,0});for(int i=0;i<128;++i)t.createMap({0,80,0});log("map_limit_safe",t.createMap({0,80,0})==0);log("map_reference_reclamation",t.reclaimMaps({1,64})==126 && t.map(1) && t.map(64) && t.createMap({0,80,0})==129);
 }
 glfwDestroyWindow(window);glfwTerminate();return 0;
}




int StabilityTest::traversal(int rd,const std::filesystem::path& output){
 std::filesystem::create_directories(output);std::ofstream csv(output/("survival-rd"+std::to_string(rd)+".csv"));
 std::ofstream rates(output/("timing-rd"+std::to_string(rd)+".csv"));rates<<"target_fps,actual_fps,wall_seconds,simulation_seconds,debt_seconds,unaccounted_seconds\n";
 csv<<"phase,rd,frames,fps,mean_ms,p95_ms,p99_ms,worst_ms,mean_work_ms,ram_mb,terrain_bytes,distance,occlusion_chunks,save_capture_ms,save_total_ms\n";
 Game g;char app[]="StabilityDriver",seed[]="--seed=123456789";char* args[]={app,seed};if(!g.initialize(2,args))return 1;
 g.settings_.renderDistance=rd;g.settings_.simulationDistance=8;g.settings_.terrainLod=0;g.settings_.occlusionCulling=1;g.settings_.antiAliasingSamples=0;g.settings_.vsync=false;glfwSwapInterval(0);
 const auto saved=WorldLibrary::create("Survival traversal",123456789,GameMode::Survival,false);g.playSavedWorld(saved);if(!g.world_)return 2;
 for(int i=0;i<4;++i){g.inventory_->add(Item(int(Item::IronHelmet)+i));g.inventory_->equipFromSlot(0);}g.inventory_->add(Item::Shield);g.inventory_->equipFromSlot(0);
 g.inventory_->add(Item::IronSword);const auto mapId=g.transportation_->createMap(g.player_->position());g.inventory_->add(Item::FilledMap,1,0,mapId);g.inventory_->selectSlot(1);
 g.transportation_->addWaypoint("Starting camp",g.player_->position());g.weather_->set(WeatherType::Rain,300);
 const auto warm=glfwGetTime();g.timing_.previousFrame=warm;while(glfwGetTime()-warm<5){const auto dt=g.beginFrame();g.updateSimulation(dt);g.renderFrame(dt);}
 for(int phase=0;phase<2;++phase){
  if(phase==1){bool placed=false;
   for(const auto& p:std::array<glm::vec3,6>{{{402.5f,69.5f,-2815.5f},{202.5f,68.5f,-2892.5f},{92.5f,67.5f,-2909.5f},{-1967.5f,64.5f,-2059.5f},{-1148.5f,64.5f,-1997.5f},{-584.5f,65.5f,-2263.5f}}}){
    g.world_->prepareSpawnTerrain(p);if(g.transportation_->placeBoat(*g.world_,Item::OakBoat,p,0)){const auto& b=g.transportation_->boats().back();g.player_->teleport(b.position+glm::vec3(-1.8f,1,0));placed=g.transportation_->useBoat(*g.player_,*g.world_,b.position+glm::vec3(0,1,0),{0,-1,0},4);if(placed)break;}}
   if(!placed)throw std::runtime_error("No validated V22 river boat fixture");
  }
  std::vector<double> frames;double work=0;std::uint64_t culled=0;bool shot=false,spike=false;const auto origin=g.player_->position();
  const double start=glfwGetTime(),duration=phase?15:20;g.timing_.previousFrame=start;
  const std::array<int,6> ratesToTest{10,15,20,30,60,120};
  int bucket=0,bucketFrames=0;double bucketStart=start,bucketDebt=g.simulationClock_.debt;float bucketSim=g.timing_.worldTime;
  auto recordBucket=[&](double snapshot){const double wall=snapshot-bucketStart,sim=g.timing_.worldTime-bucketSim;
   const double missing=wall-sim-g.simulationClock_.debt+bucketDebt;
   rates<<ratesToTest[bucket]<<','<<bucketFrames/wall<<','<<wall<<','<<sim<<','<<g.simulationClock_.debt<<','<<missing<<'\n';
   if(std::abs(missing)>.01)throw std::runtime_error("Wall/simulation time accounting drift");
  };
  HANDLE limiter=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
  while(glfwGetTime()-start<duration){const auto frameStart=glfwGetTime();const float dt=g.beginFrame();
   const int currentBucket=phase?0:std::min(5,int((frameStart-start)/3));
   g.input_.keys_[GLFW_KEY_W]=true;g.input_.keys_[GLFW_KEY_SPACE]=phase==0 && int(frameStart-start)%3==0;
   g.input_.keys_[GLFW_KEY_A]=phase==1 && int(frameStart-start)%6<2;
   if(phase==0)g.player_->addMouseMovement(30*dt,0);
   g.sounds_->update(dt);g.pollSave();g.updateSimulation(dt);
   if(!phase && currentBucket!=bucket){recordBucket(g.timing_.previousFrame);bucket=currentBucket;bucketFrames=0;bucketStart=g.timing_.previousFrame;bucketSim=g.timing_.worldTime;bucketDebt=g.simulationClock_.debt;}
   ++bucketFrames;
   if(!shot && frameStart-start>5){g.screenshotRequested_=true;shot=true;}
   g.renderFrame(dt);culled+=g.renderer_->occlusionCulledCount();frames.push_back(dt*1000);work+=(glfwGetTime()-frameStart)*1000;
   if(!spike && frameStart-start>9){std::this_thread::sleep_for(std::chrono::milliseconds(500));spike=true;}
   const auto remaining=frameStart+1.0/(phase?60:ratesToTest[currentBucket])-glfwGetTime();
   if(remaining>0){if(limiter){LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(remaining*10000000));SetWaitableTimer(limiter,&due,0,nullptr,nullptr,FALSE);WaitForSingleObject(limiter,1000);}else std::this_thread::sleep_for(std::chrono::duration<double>(remaining));}
  }
  if(limiter)CloseHandle(limiter);
  if(!phase)recordBucket(g.timing_.previousFrame);
  const auto elapsed=glfwGetTime()-start;std::sort(frames.begin(),frames.end());double ram=0;
#ifdef _WIN32
  PROCESS_MEMORY_COUNTERS pm{};GetProcessMemoryInfo(GetCurrentProcess(),&pm,sizeof(pm));ram=pm.WorkingSetSize/1048576.;
#endif
  const auto saveStart=glfwGetTime();g.saveAll(true);const auto capture=glfwGetTime();g.pollSave(true);const auto complete=glfwGetTime();
  csv<<(phase?"boat":"walking")<<','<<rd<<','<<frames.size()<<','<<frames.size()/elapsed<<','<<elapsed*1000/frames.size()<<','<<frames[(frames.size()-1)*95/100]<<','<<frames[(frames.size()-1)*99/100]<<','<<frames.back()<<','<<work/frames.size()<<','<<ram<<','<<g.world_->terrainMemory().capacityBytes<<','<<glm::length(g.player_->position()-origin)<<','<<culled<<','<<(capture-saveStart)*1000<<','<<(complete-saveStart)*1000<<'\n';csv.flush();
 }
 g.returnToMainMenu();g.playSavedWorld(saved);if(!g.world_ || !g.transportation_->map(mapId) || g.transportation_->boats().empty())throw std::runtime_error("Survival traversal reload");g.returnToMainMenu();
 for(const auto& file:std::filesystem::directory_iterator("screenshots"))std::filesystem::copy_file(file.path(),output/("rd"+std::to_string(rd)+"-"+file.path().filename().string()),std::filesystem::copy_options::overwrite_existing);
 g.shutdown();std::cout<<"Production Survival traversal/boat/weather/map/equipment checkpoint passed RD"<<rd<<std::endl;return 0;
}
